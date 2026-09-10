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
#define COLOR_CARD    0xFF191919
#define COLOR_SOFT    0xFF282828
#define COLOR_BLACK   0xFF000000
#define COLOR_WHITE   0xFFFFFFFF

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

static void drawTextSmall(int x, int y, const char *text, u32 color)
{
    int i = 0;

    while (text[i] != '\0') {
        drawChar(x + i * 6, y, text[i], color, 1);
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
    const char *title1;
    const char *title2;
    const char *channel;
    const char *views;
    const char *duration;
} Video;

static const Video videos[] = {
    {"WELCOME TO PSP", "YOUTUBE", "PSP CHANNEL", "2.4M", "12:34"},
    {"LATEST GAMING", "VIDEOS", "GAMING", "5.1M", "15:42"},
    {"BEAUTIFUL PLACES", "IN 4K", "NATURE", "1.8M", "10:28"},
    {"RETRO GAMES", "COLLECTION", "RETRO", "920K", "18:05"},
    {"TECHNOLOGY", "NEWS", "TECH", "3.7M", "08:16"},
    {"NEW PSP", "PROJECTS", "HOMEBREW", "640K", "04:46"}
};

#define VIDEO_COUNT 6

enum {
    PAGE_HOME = 0,
    PAGE_TRENDING,
    PAGE_SHORTS,
    PAGE_SEARCH,
    PAGE_SETTINGS,
    PAGE_LIBRARY,
    PAGE_VIDEO
};

static void drawPlayTriangle(int cx, int cy, int size, u32 color)
{
    Vertex *v = sceGuGetMemory(3 * sizeof(Vertex));
    v[0].color = color; v[0].x = cx - size / 3; v[0].y = cy - size; v[0].z = 0;
    v[1].color = color; v[1].x = cx - size / 3; v[1].y = cy + size; v[1].z = 0;
    v[2].color = color; v[2].x = cx + size;     v[2].y = cy;          v[2].z = 0;
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        3, NULL, v);
}

static void drawThumbnail(int x, int y, int w, int h, int index, int selected)
{
    /* Modern PSP-safe thumbnail: layered rectangles, no textures required. */
    drawRect(x, y, w, h, selected ? COLOR_RED : COLOR_SOFT);
    drawRect(x + 2, y + 2, w - 4, h - 4, COLOR_CARD);

    /* Stylized video preview. Different bands make the placeholder feel like artwork. */
    drawRect(x + 6, y + 6, w - 12, h - 12, COLOR_BLACK);
    drawRect(x + 8, y + 8, w - 16, 7, COLOR_RED);
    drawRect(x + 8, y + 20, w - 16, 18, COLOR_PANEL2);
    drawRect(x + 8, y + 41, w - 16, 9, COLOR_SOFT);
    drawRect(x + 8, y + 53, (w - 16) * 2 / 3, 8, COLOR_PANEL);
    drawRect(x + 8, y + h - 17, w - 16, 8, COLOR_PANEL2);

    drawPlayTriangle(x + w / 2, y + h / 2, 12, COLOR_TEXT);

    /* Duration badge. */
    drawRect(x + w - 50, y + h - 23, 44, 18, COLOR_BLACK);
    drawTextSmall(x + w - 45, y + h - 16, videos[index].duration, COLOR_TEXT);

    /* Selected-state accent. */
    if (selected)
        drawRect(x + 2, y + h - 4, w - 4, 2, COLOR_RED);
}

static void drawHeader(int page)
{
    const char *title;

    title = "HOME";
    if (page == PAGE_TRENDING) title = "TRENDING";
    else if (page == PAGE_SHORTS) title = "SHORTS";
    else if (page == PAGE_SEARCH) title = "SEARCH";
    else if (page == PAGE_SETTINGS) title = "SETTINGS";
    else if (page == PAGE_LIBRARY) title = "LIBRARY";
    else if (page == PAGE_VIDEO) title = "VIDEO";

    drawRect(0, 0, SCREEN_WIDTH, 40, COLOR_PANEL);
    drawRect(0, 38, SCREEN_WIDTH, 2, COLOR_RED);

    /* L button and hamburger, matching the user's sketch. */
    drawBorder(7, 8, 24, 22, 1, COLOR_BORDER);
    drawText(15, 14, "L", COLOR_TEXT);

    drawRect(43, 12, 18, 2, COLOR_TEXT);
    drawRect(43, 18, 18, 2, COLOR_TEXT);
    drawRect(43, 24, 18, 2, COLOR_TEXT);

    /* PSP YouTube branding. */
    drawRect(69, 9, 27, 21, COLOR_RED);
    drawPlayTriangle(83, 19, 5, COLOR_WHITE);
    drawText(103, 12, "PSP", COLOR_TEXT);
    drawText(127, 12, "YOUTUBE", COLOR_RED);

    /* Page title in the center. */
    drawTextSmall(205, 15, title, COLOR_TEXT);

    /* Search control and R shortcut. */
    drawBorder(351, 8, 82, 22, 1, COLOR_BORDER);
    drawTextSmall(361, 15, "SEARCH", COLOR_TEXT);

    drawBorder(442, 8, 29, 22, 1, COLOR_BORDER);
    drawText(451, 14, "R", COLOR_TEXT);
}

static void drawVideoCard(int index, int x, int y, int w, int selected)
{
    int thumbH;

    thumbH = 76;

    drawRect(x, y, w, 104, COLOR_CARD);

    if (selected) {
        drawRect(x, y, w, 3, COLOR_RED);
        drawRect(x, y + 101, w, 3, COLOR_RED);
    }

    drawThumbnail(x + 6, y + 6, w - 12, thumbH, index, selected);

    drawTextSmall(x + 7, y + 84, videos[index].title1, COLOR_TEXT);
    drawTextSmall(x + 7, y + 95, videos[index].title2, COLOR_TEXT);

    /* Compact YouTube-like metadata indicator. */
    drawRect(x + w - 30, y + 88, 3, 3, selected ? COLOR_RED : COLOR_DIM);
    drawTextSmall(x + w - 22, y + 84, videos[index].views, COLOR_DIM);
}

static void drawHome(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_HOME);

    drawTextSmall(10, 46, "RECOMMENDED", COLOR_DIM);

    drawVideoCard(0, 8, 55, 227, selectedVideo == 0);
    drawVideoCard(1, 245, 55, 227, selectedVideo == 1);
    drawVideoCard(2, 8, 164, 227, selectedVideo == 2);
    drawVideoCard(3, 245, 164, 227, selectedVideo == 3);
}

static void drawTrending(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_TRENDING);

    drawTextSmall(10, 46, "POPULAR NOW", COLOR_DIM);

    drawVideoCard(4, 8, 55, 227, selectedVideo == 4);
    drawVideoCard(5, 245, 55, 227, selectedVideo == 5);
    drawVideoCard(1, 8, 164, 227, selectedVideo == 1);
    drawVideoCard(2, 245, 164, 227, selectedVideo == 2);
}

static void drawShorts(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SHORTS);

    drawTextSmall(10, 46, "SHORT VIDEO FEED", COLOR_DIM);

    drawRect(18, 62, 135, 158, COLOR_CARD);
    drawRect(172, 62, 135, 158, COLOR_CARD);
    drawRect(326, 62, 135, 158, COLOR_CARD);

    drawRect(24, 68, 123, 112, COLOR_SOFT);
    drawRect(178, 68, 123, 112, COLOR_PANEL);
    drawRect(332, 68, 123, 112, COLOR_SOFT);

    drawPlayTriangle(86, 124, 12, COLOR_TEXT);
    drawPlayTriangle(240, 124, 12, COLOR_TEXT);
    drawPlayTriangle(394, 124, 12, COLOR_TEXT);

    drawTextSmall(27, 189, "SHORT 01", COLOR_TEXT);
    drawTextSmall(181, 189, "SHORT 02", COLOR_TEXT);
    drawTextSmall(335, 189, "SHORT 03", COLOR_TEXT);
}

static void drawSearch(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SEARCH);

    drawBorder(28, 72, 424, 42, 2, COLOR_BORDER);

    drawText(46, 89, "SEARCH YOUTUBE", COLOR_DIM);

    drawRect(28, 126, 145, 34, COLOR_RED);
    drawText(68, 139, "SEARCH", COLOR_TEXT);

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

    drawThumbnail(18, 57, 250, 126, selectedVideo, 0);

    drawText(286, 61, videos[selectedVideo].title1, COLOR_TEXT);
    drawText(286, 78, videos[selectedVideo].title2, COLOR_TEXT);
    drawTextSmall(286, 101, videos[selectedVideo].channel, COLOR_DIM);

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
        "SHORTS",
        "SETTINGS",
        "LIBRARY"
    };
    int i;

    if (x <= -SIDEBAR_WIDTH)
        return;

    drawRect(x, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, COLOR_PANEL);
    drawRect(x + SIDEBAR_WIDTH - 2, 0, 2, SCREEN_HEIGHT, COLOR_RED);

    drawRect(x + 12, 10, 27, 20, COLOR_RED);
    drawPlayTriangle(x + 26, 20, 5, COLOR_WHITE);
    drawTextSmall(x + 46, 15, "PSP YOUTUBE", COLOR_TEXT);

    drawTextSmall(x + 16, 47, "MENU", COLOR_DIM);
    drawRect(x + 10, 57, SIDEBAR_WIDTH - 20, 1, COLOR_BORDER);

    for (i = 0; i < 5; i++) {
        int y = 66 + i * 35;

        if (i == selected) {
            drawRect(x + 7, y, SIDEBAR_WIDTH - 14, 29, COLOR_RED);
            drawRect(x + 7, y, 3, 29, COLOR_WHITE);
        }

        drawTextSmall(
            x + 20,
            y + 10,
            items[i],
            (i == selected) ? COLOR_TEXT : COLOR_DIM
        );
    }

    drawRect(x + 10, 247, SIDEBAR_WIDTH - 20, 1, COLOR_BORDER);
    drawTextSmall(x + 20, 255, "L CLOSE", COLOR_DIM);
}

static int sidebarPage(int selected)
{
    if (selected == 0) return PAGE_HOME;
    if (selected == 1) return PAGE_TRENDING;
    if (selected == 2) return PAGE_SHORTS;
    if (selected == 3) return PAGE_SETTINGS;
    return PAGE_LIBRARY;
}

static int pageSidebarIndex(int page)
{
    if (page == PAGE_HOME) return 0;
    if (page == PAGE_TRENDING) return 1;
    if (page == PAGE_SHORTS) return 2;
    if (page == PAGE_SETTINGS) return 3;
    if (page == PAGE_LIBRARY) return 4;
    return 0;
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

            if (sidebarOpen)
                sidebarSelected = pageSidebarIndex(page);

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
                page = sidebarPage(sidebarSelected);
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
                /* Page-aware navigation. Trending uses the visible order 4,5,1,2. */
                if (page == PAGE_HOME) {
                    if (pressed & PSP_CTRL_LEFT) {
                        if (selectedVideo == 1) selectedVideo = 0;
                        else if (selectedVideo == 3) selectedVideo = 2;
                    }
                    if (pressed & PSP_CTRL_RIGHT) {
                        if (selectedVideo == 0) selectedVideo = 1;
                        else if (selectedVideo == 2) selectedVideo = 3;
                    }
                    if (pressed & PSP_CTRL_UP) {
                        if (selectedVideo >= 2) selectedVideo -= 2;
                    }
                    if (pressed & PSP_CTRL_DOWN) {
                        if (selectedVideo < 2) selectedVideo += 2;
                    }
                } else {
                    if (pressed & PSP_CTRL_LEFT) {
                        if (selectedVideo == 5) selectedVideo = 4;
                        else if (selectedVideo == 2) selectedVideo = 1;
                    }
                    if (pressed & PSP_CTRL_RIGHT) {
                        if (selectedVideo == 4) selectedVideo = 5;
                        else if (selectedVideo == 1) selectedVideo = 2;
                    }
                    if (pressed & PSP_CTRL_UP) {
                        if (selectedVideo == 1) selectedVideo = 4;
                        else if (selectedVideo == 2) selectedVideo = 5;
                    }
                    if (pressed & PSP_CTRL_DOWN) {
                        if (selectedVideo == 4) selectedVideo = 1;
                        else if (selectedVideo == 5) selectedVideo = 2;
                    }
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
            else if (page == PAGE_SHORTS) {
                if (pressed & PSP_CTRL_CROSS)
                    page = PAGE_VIDEO;
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

            if (pressed & PSP_CTRL_RTRIGGER)
                page = PAGE_SEARCH;

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
        else if (page == PAGE_SHORTS)
            drawShorts();
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
