#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>

PSP_MODULE_INFO("PSP YouTube", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUF_WIDTH     512

static unsigned int __attribute__((aligned(16))) list[262144];

#define RGB(r,g,b) (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))
#define COLOR_BG     RGB(16,16,16)
#define COLOR_PANEL  RGB(23,23,23)
#define COLOR_CARD   RGB(35,35,35)
#define COLOR_CARD2  RGB(43,43,43)
#define COLOR_TEXT   RGB(245,245,245)
#define COLOR_DIM    RGB(150,150,150)
#define COLOR_RED    RGB(230,35,35)
#define COLOR_SELECT RGB(52,52,52)
#define COLOR_BORDER RGB(90,90,90)

int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    if (cbid >= 0)
        sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread(
        "CallbackThread", CallbackThread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
    return thid;
}

void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void *)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - SCREEN_WIDTH / 2, 2048 - SCREEN_HEIGHT / 2);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHTING);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

typedef struct {
    float x, y, z;
} Vertex;

void drawRect(int x, int y, int width, int height, unsigned int color)
{
    Vertex *v;
    if (width <= 0 || height <= 0)
        return;

    sceGuColor(color);
    v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].x = (float)x;
    v[0].y = (float)y;
    v[0].z = 0.0f;
    v[1].x = (float)(x + width);
    v[1].y = (float)(y + height);
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
}

void drawBorder(int x, int y, int width, int height,
                int thickness, unsigned int color)
{
    drawRect(x, y, width, thickness, color);
    drawRect(x, y + height - thickness, width, thickness, color);
    drawRect(x, y, thickness, height, color);
    drawRect(x + width - thickness, y, thickness, height, color);
}

/*
 * Compact built-in 5x5 font.
 * Lowercase letters are converted to uppercase.
 * Arabic support will be added later.
 */
static const unsigned char font5x5[37][5] = {
    {0,0,0,0,0},       /* space */
    {14,17,31,17,17},  /* A */
    {30,17,30,17,30},  /* B */
    {14,17,16,17,14},  /* C */
    {30,17,17,17,30},  /* D */
    {31,16,30,16,31},  /* E */
    {31,16,30,16,16},  /* F */
    {14,16,23,17,14},  /* G */
    {17,17,31,17,17},  /* H */
    {14,4,4,4,14},     /* I */
    {7,2,2,18,12},     /* J */
    {17,18,28,18,17},  /* K */
    {16,16,16,16,31},  /* L */
    {17,27,21,17,17},  /* M */
    {17,25,21,19,17},  /* N */
    {14,17,17,17,14},  /* O */
    {30,17,30,16,16},  /* P */
    {14,17,17,21,14},  /* Q */
    {30,17,30,18,17},  /* R */
    {15,16,14,1,30},   /* S */
    {31,4,4,4,4},      /* T */
    {17,17,17,17,14},  /* U */
    {17,17,17,10,4},   /* V */
    {17,17,21,27,17},  /* W */
    {17,10,4,10,17},   /* X */
    {17,10,4,4,4},     /* Y */
    {31,2,4,8,31},     /* Z */
    {14,17,19,21,14},  /* 0 */
    {4,12,4,4,14},     /* 1 */
    {14,17,2,4,31},    /* 2 */
    {30,1,6,1,30},     /* 3 */
    {2,6,10,31,2},     /* 4 */
    {31,16,30,1,30},   /* 5 */
    {6,8,30,17,14},    /* 6 */
    {31,1,2,4,8},      /* 7 */
    {14,17,14,17,14},  /* 8 */
    {14,17,15,1,14}    /* 9 */
};

int fontIndex(char c)
{
    if (c == ' ')
        return 0;

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    if (c >= 'A' && c <= 'Z')
        return 1 + (c - 'A');

    if (c >= '0' && c <= '9')
        return 27 + (c - '0');

    return 0;
}

void drawChar(int x, int y, char c, int scale, unsigned int color)
{
    int row, col;
    int idx = fontIndex(c);

    for (row = 0; row < 5; row++) {
        unsigned char bits = font5x5[idx][row];
        for (col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col)))
                drawRect(x + col * scale, y + row * scale,
                         scale, scale, color);
        }
    }
}

void drawText(int x, int y, const char *text, unsigned int color)
{
    int i = 0;
    while (text[i] != '\0') {
        drawChar(x + i * 6, y, text[i], 1, color);
        i++;
    }
}

void drawTextLarge(int x, int y, const char *text, unsigned int color)
{
    int i = 0;
    while (text[i] != '\0') {
        drawChar(x + i * 12, y, text[i], 2, color);
        i++;
    }
}

typedef struct {
    const char *title;
    const char *channel;
} Video;

static Video videos[] = {
    {"Welcome to PSP YouTube", "PSP Channel"},
    {"Latest Gaming Videos", "Gaming"},
    {"PSP Homebrew News", "PSP Dev"},
    {"Retro Games", "Retro"},
    {"Technology", "Tech"},
    {"New PSP Projects", "Homebrew"}
};

#define VIDEO_COUNT 6

void drawThumbnail(int x, int y, int width, int height, int selected)
{
    drawRect(x, y, width, height, COLOR_CARD);
    drawRect(x + 1, y + 1, width - 2, height - 2, COLOR_CARD2);

    drawRect(x + 10, y + 10, width - 20, 3, COLOR_DIM);
    drawRect(x + 10, y + height - 13, width - 20, 3, COLOR_DIM);

    drawRect(x + width / 2 - 7, y + height / 2 - 14,
             7, 28, COLOR_RED);
    drawRect(x + width / 2, y + height / 2 - 10,
             7, 20, COLOR_RED);
    drawRect(x + width / 2 + 7, y + height / 2 - 5,
             7, 10, COLOR_RED);

    if (selected)
        drawBorder(x - 2, y - 2, width + 4, height + 4,
                   2, COLOR_RED);
}

void drawSidebar(int selectedMenu)
{
    const char *items[] = {
        "HOME", "TRENDING", "SEARCH", "LIBRARY", "SETTINGS"
    };
    int i, y = 75;

    drawRect(0, 0, 125, SCREEN_HEIGHT, COLOR_PANEL);
    drawRect(124, 0, 1, SCREEN_HEIGHT, COLOR_BORDER);

    drawRect(15, 13, 31, 21, COLOR_RED);
    drawRect(28, 18, 8, 11, COLOR_TEXT);

    drawTextLarge(52, 15, "PSP", COLOR_TEXT);
    drawText(15, 42, "YOUTUBE", COLOR_DIM);

    for (i = 0; i < 5; i++) {
        if (i == selectedMenu) {
            drawRect(8, y - 8, 108, 25, COLOR_SELECT);
            drawRect(8, y - 8, 3, 25, COLOR_RED);
        }

        drawText(20, y, items[i],
                 i == selectedMenu ? COLOR_TEXT : COLOR_DIM);
        y += 32;
    }
}

void drawHeader(int selectedMenu)
{
    const char *titles[] = {
        "HOME", "TRENDING", "SEARCH", "LIBRARY", "SETTINGS"
    };

    drawRect(125, 0, SCREEN_WIDTH - 125, 45, COLOR_PANEL);
    drawTextLarge(145, 13, titles[selectedMenu], COLOR_TEXT);

    drawBorder(345, 10, 82, 22, 1, COLOR_BORDER);
    drawText(355, 17, "SEARCH", COLOR_DIM);

    drawBorder(438, 10, 12, 12, 2, COLOR_TEXT);
    drawRect(449, 21, 6, 2, COLOR_TEXT);
}

void drawHome(int selectedMenu, int selectedVideo)
{
    const int cardWidth = 150;
    const int cardHeight = 70;
    const int x1 = 145, x2 = 315;
    const int y1 = 70, y2 = 171;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawSidebar(selectedMenu);
    drawHeader(selectedMenu);

    drawTextLarge(145, 53, "RECOMMENDED", COLOR_TEXT);

    drawThumbnail(x1, y1, cardWidth, cardHeight, selectedVideo == 0);
    drawText(x1, y1 + cardHeight + 7, videos[0].title, COLOR_TEXT);
    drawText(x1, y1 + cardHeight + 18, videos[0].channel, COLOR_DIM);

    drawThumbnail(x2, y1, cardWidth, cardHeight, selectedVideo == 1);
    drawText(x2, y1 + cardHeight + 7, videos[1].title, COLOR_TEXT);
    drawText(x2, y1 + cardHeight + 18, videos[1].channel, COLOR_DIM);

    drawThumbnail(x1, y2, cardWidth, cardHeight, selectedVideo == 2);
    drawText(x1, y2 + cardHeight + 7, videos[2].title, COLOR_TEXT);
    drawText(x1, y2 + cardHeight + 18, videos[2].channel, COLOR_DIM);

    drawThumbnail(x2, y2, cardWidth, cardHeight, selectedVideo == 3);
    drawText(x2, y2 + cardHeight + 7, videos[3].title, COLOR_TEXT);
    drawText(x2, y2 + cardHeight + 18, videos[3].channel, COLOR_DIM);
}

void drawPlaceholder(int selectedMenu)
{
    const char *titles[] = {
        "HOME", "TRENDING", "SEARCH", "LIBRARY", "SETTINGS"
    };

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawSidebar(selectedMenu);
    drawHeader(selectedMenu);

    drawTextLarge(160, 105, titles[selectedMenu], COLOR_TEXT);
    drawText(160, 135, "COMING SOON", COLOR_DIM);
}

void waitForVblank(void)
{
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void)
{
    SceCtrlData pad;
    SceCtrlData oldPad;

    int selectedMenu = 0;
    int selectedVideo = 0;
    int focus = 0; /* 0 = sidebar, 1 = video grid */

    memset(&oldPad, 0, sizeof(oldPad));

    SetupCallbacks();
    initGraphics();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (focus == 0) {
            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(oldPad.Buttons & PSP_CTRL_DOWN)) {
                if (selectedMenu < 4)
                    selectedMenu++;
            }

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(oldPad.Buttons & PSP_CTRL_UP)) {
                if (selectedMenu > 0)
                    selectedMenu--;
            }

            if ((pad.Buttons & PSP_CTRL_RIGHT) &&
                !(oldPad.Buttons & PSP_CTRL_RIGHT)) {
                if (selectedMenu == 0)
                    focus = 1;
            }

            if ((pad.Buttons & PSP_CTRL_CIRCLE) &&
                !(oldPad.Buttons & PSP_CTRL_CIRCLE)) {
                sceKernelExitGame();
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldPad.Buttons & PSP_CTRL_CROSS)) {
                if (selectedMenu != 0) {
                    /* Other pages are intentionally placeholders for 0.2v. */
                }
            }
        } else {
            if ((pad.Buttons & PSP_CTRL_LEFT) &&
                !(oldPad.Buttons & PSP_CTRL_LEFT)) {
                focus = 0;
            }

            if ((pad.Buttons & PSP_CTRL_RIGHT) &&
                !(oldPad.Buttons & PSP_CTRL_RIGHT)) {
                if ((selectedVideo % 2) == 0 && selectedVideo + 1 < 4)
                    selectedVideo++;
            }

            if ((pad.Buttons & PSP_CTRL_LEFT) &&
                !(oldPad.Buttons & PSP_CTRL_LEFT)) {
                if ((selectedVideo % 2) == 1)
                    selectedVideo--;
                else
                    focus = 0;
            }

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(oldPad.Buttons & PSP_CTRL_DOWN)) {
                if (selectedVideo + 2 < 4)
                    selectedVideo += 2;
            }

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(oldPad.Buttons & PSP_CTRL_UP)) {
                if (selectedVideo >= 2)
                    selectedVideo -= 2;
            }

            if ((pad.Buttons & PSP_CTRL_CIRCLE) &&
                !(oldPad.Buttons & PSP_CTRL_CIRCLE)) {
                focus = 0;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldPad.Buttons & PSP_CTRL_CROSS)) {
                /*
                 * Video selection placeholder.
                 * Internet/video playback will be implemented later.
                 */
            }
        }

        sceGuStart(GU_DIRECT, list);

        if (selectedMenu == 0)
            drawHome(selectedMenu, selectedVideo);
        else
            drawPlaceholder(selectedMenu);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        oldPad = pad;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}