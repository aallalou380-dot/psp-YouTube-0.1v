#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>

PSP_MODULE_INFO("PSP YouTube 0.2v", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUF_WIDTH     512

#define COLOR_BG      0xFF101010
#define COLOR_PANEL   0xFF202020
#define COLOR_PANEL2  0xFF303030
#define COLOR_TEXT    0xFFFFFFFF
#define COLOR_DIM     0xFF909090
#define COLOR_BORDER  0xFF606060
#define COLOR_RED     0xFF0000FF
#define COLOR_BLACK   0xFF000000

#define SIDEBAR_WIDTH 125
#define SIDEBAR_SPEED 20

typedef struct {
    u32 color;
    short x;
    short y;
    short z;
} Vertex;

static unsigned int __attribute__((aligned(16))) list[262144];
static volatile int running = 1;

static int exitCallback(int arg1, int arg2, void *common)
{
    running = 0;
    return 0;
}

static int callbackThread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);

    if (cbid >= 0)
        sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();
    return 0;
}

static int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread(
        "update_thread",
        callbackThread,
        0x11,
        0xFA0,
        0,
        NULL
    );

    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);

    return thid;
}

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void *)0x110000, BUF_WIDTH);

    sceGuOffset(
        2048 - (SCREEN_WIDTH / 2),
        2048 - (SCREEN_HEIGHT / 2)
    );

    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    /*
     * The interface is currently made entirely from opaque rectangles.
     * Disable blending for maximum compatibility with the real PSP.
     */
    sceGuDisable(GU_BLEND);

    sceGuClearColor(COLOR_BG);
    sceGuClearDepth(0);

    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void drawRect(int x, int y, int w, int h, u32 color)
{
    Vertex *v;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2,
        NULL,
        v
    );
}

static void drawBorder(int x, int y, int w, int h, int thickness, u32 color)
{
    drawRect(x, y, w, thickness, color);
    drawRect(x, y + h - thickness, w, thickness, color);
    drawRect(x, y, thickness, h, color);
    drawRect(x + w - thickness, y, thickness, h, color);
}

/*
 * Cleaner 7x9 bitmap font.
 * This keeps the PSP-friendly rectangle renderer, but gives the UI
 * more detail and smoother letter shapes than the original 5x7 font.
 */
static const unsigned char font7x9[36][9] = {
    {0x00,0x1C,0x1C,0x1C,0x36,0x36,0x3E,0x63,0x00}, /* A */
    {0x00,0x3C,0x36,0x36,0x3C,0x36,0x36,0x3E,0x00}, /* B */
    {0x00,0x1E,0x32,0x20,0x60,0x20,0x32,0x1E,0x00}, /* C */
    {0x00,0x3C,0x36,0x33,0x33,0x33,0x36,0x3C,0x00}, /* D */
    {0x00,0x3C,0x30,0x30,0x3C,0x30,0x30,0x3C,0x00}, /* E */
    {0x00,0x3C,0x30,0x30,0x3C,0x30,0x30,0x30,0x00}, /* F */
    {0x00,0x1E,0x30,0x20,0x67,0x23,0x33,0x1E,0x00}, /* G */
    {0x00,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
    {0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00}, /* I */
    {0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x08,0x18}, /* J */
    {0x00,0x6C,0x68,0x78,0x70,0x78,0x6C,0x66,0x00}, /* K */
    {0x00,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* L */
    {0x00,0x63,0x77,0x77,0x5D,0x5D,0x49,0x41,0x00}, /* M */
    {0x00,0x66,0x66,0x76,0x56,0x4E,0x4E,0x46,0x00}, /* N */
    {0x00,0x3C,0x66,0x46,0x46,0x46,0x66,0x3C,0x00}, /* O */
    {0x00,0x3C,0x36,0x36,0x3C,0x30,0x30,0x30,0x00}, /* P */
    {0x3C,0x66,0x46,0x46,0x46,0x66,0x3C,0x04,0x00}, /* Q */
    {0x00,0x3C,0x36,0x36,0x3C,0x36,0x36,0x33,0x00}, /* R */
    {0x00,0x3C,0x20,0x30,0x3C,0x06,0x26,0x3C,0x00}, /* S */
    {0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* T */
    {0x00,0x32,0x32,0x32,0x32,0x32,0x32,0x1C,0x00}, /* U */
    {0x00,0x63,0x22,0x36,0x36,0x1C,0x1C,0x1C,0x00}, /* V */
    {0x00,0x19,0x19,0x1B,0x7F,0x67,0x66,0x66,0x00}, /* W */
    {0x00,0x22,0x36,0x1C,0x1C,0x1C,0x36,0x62,0x00}, /* X */
    {0x00,0x66,0x36,0x3C,0x18,0x18,0x18,0x18,0x00}, /* Y */
    {0x00,0x7E,0x06,0x0C,0x18,0x38,0x30,0x7E,0x00}, /* Z */
    {0x00,0x1C,0x24,0x26,0x66,0x26,0x24,0x1C,0x00}, /* 0 */
    {0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00}, /* 1 */
    {0x00,0x3C,0x04,0x04,0x0C,0x18,0x30,0x3C,0x00}, /* 2 */
    {0x00,0x3C,0x0C,0x0C,0x1C,0x04,0x04,0x3C,0x00}, /* 3 */
    {0x00,0x0C,0x1C,0x1C,0x2C,0x6C,0x7E,0x0C,0x00}, /* 4 */
    {0x00,0x3C,0x20,0x20,0x3C,0x06,0x06,0x3C,0x00}, /* 5 */
    {0x00,0x1C,0x30,0x20,0x3C,0x36,0x36,0x1C,0x00}, /* 6 */
    {0x00,0x3E,0x04,0x0C,0x08,0x18,0x18,0x10,0x00}, /* 7 */
    {0x00,0x3C,0x24,0x34,0x3C,0x26,0x26,0x3C,0x00}, /* 8 */
    {0x00,0x38,0x24,0x26,0x3E,0x06,0x0C,0x38,0x00}  /* 9 */
};

static int fontIndex(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';

    if (c >= '0' && c <= '9')
        return 26 + (c - '0');

    return -1;
}

static void drawChar(int x, int y, char c, u32 color, int scale)
{
    int i, j;
    int index = fontIndex(c);

    if (index < 0)
        return;

    if (scale < 1)
        scale = 1;

    for (i = 0; i < 9; i++) {
        for (j = 0; j < 7; j++) {
            if (font7x9[index][i] & (1 << (6 - j))) {
                drawRect(
                    x + j * scale,
                    y + i * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

static void drawText(int x, int y, const char *text, u32 color)
{
    int i = 0;

    while (text[i] != '\0') {
        drawChar(x + i * 8, y, text[i], color, 1);
        i++;
    }
}

static void drawTextLarge(int x, int y, const char *text, u32 color)
{
    int i = 0;

    while (text[i] != '\0') {
        drawChar(x + i * 16, y, text[i], color, 2);
        i++;
    }
}

typedef struct {
    const char *title;
    const char *channel;
    const char *duration;
} Video;

static const Video videos[] = {
    {"WELCOME TO PSP YOUTUBE", "PSP CHANNEL", "2:48"},
    {"LATEST GAMING VIDEOS", "GAMING", "8:21"},
    {"PSP HOMEBREW NEWS", "PSP DEV", "5:32"},
    {"RETRO GAMES", "RETRO", "10:05"},
    {"TECHNOLOGY", "TECH", "6:17"},
    {"NEW PSP PROJECTS", "HOMEBREW", "4:46"}
};

#define VIDEO_COUNT 6

enum {
    PAGE_HOME = 0,
    PAGE_TRENDING,
    PAGE_SEARCH,
    PAGE_LIBRARY,
    PAGE_SETTINGS,
    PAGE_VIDEO
};

static void drawThumbnail(int x, int y, int w, int h, int selected, const char *duration)
{
    /* Keep the thumbnail rendering rectangle-based for PSP compatibility. */
    drawRect(x, y, w, h, selected ? COLOR_RED : COLOR_BORDER);
    drawRect(x + 3, y + 3, w - 6, h - 6, COLOR_PANEL2);

    /* Inner preview area. */
    drawRect(x + 7, y + 7, w - 14, h - 14, COLOR_BLACK);

    /* Simple play symbol. */
    drawRect(x + w / 2 - 3, y + h / 2 - 15, 6, 30, COLOR_TEXT);
    drawRect(x + w / 2 + 3, y + h / 2 - 10, 6, 20, COLOR_TEXT);
    drawRect(x + w / 2 + 9, y + h / 2 - 5, 6, 10, COLOR_TEXT);

    /* Duration badge in the lower-right corner. */
    drawRect(x + w - 48, y + h - 22, 40, 17, COLOR_BLACK);
    drawText(x + w - 42, y + h - 18, duration, COLOR_TEXT);

    /* Small red accent inside every thumbnail. */
    drawRect(x + 9, y + 9, 42, 3, COLOR_RED);
}

static void drawHeader(int page)
{
    const char *title = "HOME";

    if (page == PAGE_TRENDING)
        title = "TRENDING";
    else if (page == PAGE_SEARCH)
        title = "SEARCH";
    else if (page == PAGE_LIBRARY)
        title = "LIBRARY";
    else if (page == PAGE_SETTINGS)
        title = "SETTINGS";
    else if (page == PAGE_VIDEO)
        title = "VIDEO";

    drawRect(0, 0, SCREEN_WIDTH, 45, COLOR_PANEL);

    drawTextLarge(18, 12, "PSP YOUTUBE", COLOR_TEXT);
    drawText(370, 18, title, COLOR_DIM);

    drawRect(0, 44, SCREEN_WIDTH, 1, COLOR_BORDER);
}

static void drawVideoCard(int index, int x, int y, int w, int h, int selected)
{
    drawThumbnail(x, y, w, h, selected, videos[index].duration);

    drawText(x, y + h + 5, videos[index].title, COLOR_TEXT);
    drawText(x, y + h + 16, videos[index].channel, COLOR_DIM);
}

static void drawHome(int selectedVideo)
{
    int x1 = 20;
    int x2 = 250;
    int y1 = 62;
    int y2 = 167;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_HOME);

    drawVideoCard(0, x1, y1, 205, 72, selectedVideo == 0);
    drawVideoCard(1, x2, y1, 205, 72, selectedVideo == 1);
    drawVideoCard(2, x1, y2, 205, 72, selectedVideo == 2);
    drawVideoCard(3, x2, y2, 205, 72, selectedVideo == 3);
}

static void drawTrending(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_TRENDING);

    drawVideoCard(4, 20, 62, 205, 72, selectedVideo == 4);
    drawVideoCard(5, 250, 62, 205, 72, selectedVideo == 5);
    drawVideoCard(1, 20, 167, 205, 72, selectedVideo == 1);
    drawVideoCard(2, 250, 167, 205, 72, selectedVideo == 2);
}

static void drawSearch(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SEARCH);

    drawBorder(35, 70, 410, 42, 2, COLOR_BORDER);

    drawText(52, 87, "SEARCH YOUTUBE", COLOR_DIM);

    drawRect(35, 130, 125, 32, COLOR_RED);
    drawText(67, 142, "SEARCH", COLOR_TEXT);

    drawText(35, 190, "PRESS X TO START SEARCH", COLOR_DIM);
}

static void drawLibrary(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_LIBRARY);

    drawTextLarge(30, 80, "LIBRARY", COLOR_TEXT);

    drawText(30, 120, "YOUR VIDEOS WILL APPEAR HERE", COLOR_DIM);
    drawText(30, 150, "NETWORK FEATURES COMING LATER", COLOR_DIM);
}

static void drawSettings(int selectedSetting)
{
    const char *items[] = {
        "GENERAL",
        "VIDEO",
        "NETWORK",
        "ABOUT"
    };

    int i;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SETTINGS);

    for (i = 0; i < 4; i++) {
        u32 c = (i == selectedSetting) ? COLOR_RED : COLOR_PANEL;

        drawRect(30, 65 + i * 42, 420, 34, c);

        drawText(
            48,
            78 + i * 42,
            items[i],
            (i == selectedSetting) ? COLOR_TEXT : COLOR_DIM
        );
    }

    drawText(30, 245, "O BACK", COLOR_DIM);
}

static void drawVideoDetails(int selectedVideo)
{
    if (selectedVideo < 0)
        selectedVideo = 0;

    if (selectedVideo >= VIDEO_COUNT)
        selectedVideo = VIDEO_COUNT - 1;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_VIDEO);

    drawThumbnail(20, 60, 250, 120, 0, videos[selectedVideo].duration);

    drawTextLarge(290, 65, "VIDEO", COLOR_TEXT);
    drawText(290, 95, videos[selectedVideo].title, COLOR_TEXT);
    drawText(290, 115, videos[selectedVideo].channel, COLOR_DIM);

    drawRect(290, 140, 120, 32, COLOR_RED);
    drawText(325, 152, "PLAY", COLOR_TEXT);

    drawBorder(20, 195, 440, 55, 1, COLOR_BORDER);

    drawText(30, 207, "VIDEO DETAILS", COLOR_TEXT);
    drawText(30, 228, "PLAYBACK WILL BE ADDED LATER", COLOR_DIM);
}

static void drawSidebar(int x, int selected)
{
    const char *items[] = {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS"
    };

    int i;

    if (x <= -SIDEBAR_WIDTH)
        return;

    drawRect(x, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, COLOR_PANEL);

    drawRect(
        x + SIDEBAR_WIDTH - 1,
        0,
        1,
        SCREEN_HEIGHT,
        COLOR_BORDER
    );

    drawTextLarge(x + 15, 18, "MENU", COLOR_TEXT);

    for (i = 0; i < 5; i++) {
        int y = 60 + i * 40;

        if (i == selected) {
            drawRect(
                x + 8,
                y - 5,
                SIDEBAR_WIDTH - 16,
                30,
                COLOR_RED
            );
        }

        drawText(
            x + 18,
            y + 3,
            items[i],
            i == selected ? COLOR_TEXT : COLOR_DIM
        );
    }

    drawText(x + 18, 245, "L CLOSE", COLOR_DIM);
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    int page = PAGE_HOME;
    int selectedVideo = 0;
    int selectedSetting = 0;

    int sidebarOpen = 0;
    int sidebarSelected = 0;

    int sidebarX = -SIDEBAR_WIDTH;
    int sidebarTarget = -SIDEBAR_WIDTH;

    SetupCallbacks();
    initGraphics();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (running) {
        unsigned int pressed;

        sceCtrlReadBufferPositive(&pad, 1);

        pressed = pad.Buttons & ~oldButtons;
        oldButtons = pad.Buttons;

        if (pressed & PSP_CTRL_LTRIGGER) {
            sidebarOpen = !sidebarOpen;

            sidebarTarget =
                sidebarOpen ? 0 : -SIDEBAR_WIDTH;
        }

        if (sidebarOpen) {
            if (pressed & PSP_CTRL_UP) {
                sidebarSelected--;

                if (sidebarSelected < 0)
                    sidebarSelected = 4;
            }

            if (pressed & PSP_CTRL_DOWN) {
                sidebarSelected++;

                if (sidebarSelected > 4)
                    sidebarSelected = 0;
            }

            if (pressed & PSP_CTRL_CROSS) {
                page = sidebarSelected;
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }

            if (pressed & PSP_CTRL_CIRCLE) {
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }
        }
        else {
            if (page == PAGE_HOME || page == PAGE_TRENDING) {
                if (pressed & PSP_CTRL_LEFT) {
                    if (selectedVideo == 1)
                        selectedVideo = 0;
                    else if (selectedVideo == 3)
                        selectedVideo = 2;
                }

                if (pressed & PSP_CTRL_RIGHT) {
                    if (selectedVideo == 0)
                        selectedVideo = 1;
                    else if (selectedVideo == 2)
                        selectedVideo = 3;
                }

                if (pressed & PSP_CTRL_UP) {
                    if (selectedVideo >= 2)
                        selectedVideo -= 2;
                }

                if (pressed & PSP_CTRL_DOWN) {
                    if (selectedVideo < 2)
                        selectedVideo += 2;
                }

                if (pressed & PSP_CTRL_CROSS)
                    page = PAGE_VIDEO;
            }
            else if (page == PAGE_SETTINGS) {
                if (pressed & PSP_CTRL_UP) {
                    selectedSetting--;

                    if (selectedSetting < 0)
                        selectedSetting = 3;
                }

                if (pressed & PSP_CTRL_DOWN) {
                    selectedSetting++;

                    if (selectedSetting > 3)
                        selectedSetting = 0;
                }
            }
            else if (page == PAGE_SEARCH) {
                if (pressed & PSP_CTRL_CROSS) {
                    /* Real search will be added later. */
                }
            }
            else if (page == PAGE_VIDEO) {
                if (pressed & PSP_CTRL_CROSS) {
                    /* Real playback will be added later. */
                }
            }

            if (pressed & PSP_CTRL_CIRCLE)
                page = PAGE_HOME;
        }

        if (sidebarX < sidebarTarget) {
            sidebarX += SIDEBAR_SPEED;

            if (sidebarX > sidebarTarget)
                sidebarX = sidebarTarget;
        }
        else if (sidebarX > sidebarTarget) {
            sidebarX -= SIDEBAR_SPEED;

            if (sidebarX < sidebarTarget)
                sidebarX = sidebarTarget;
        }

        sceGuStart(GU_DIRECT, list);

        sceGuClearColor(COLOR_BG);
        sceGuClear(
            GU_COLOR_BUFFER_BIT |
            GU_DEPTH_BUFFER_BIT
        );

        if (page == PAGE_HOME)
            drawHome(selectedVideo);
        else if (page == PAGE_TRENDING)
            drawTrending(selectedVideo);
        else if (page == PAGE_SEARCH)
            drawSearch();
        else if (page == PAGE_LIBRARY)
            drawLibrary();
        else if (page == PAGE_SETTINGS)
            drawSettings(selectedSetting);
        else if (page == PAGE_VIDEO)
            drawVideoDetails(selectedVideo);

        drawSidebar(sidebarX, sidebarSelected);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        sceKernelDelayThread(1000);
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
