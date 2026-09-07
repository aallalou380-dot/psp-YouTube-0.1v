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

#define COLOR_BG      0x00101010
#define COLOR_PANEL   0x00202020
#define COLOR_PANEL2  0x00303030
#define COLOR_TEXT    0x00FFFFFF
#define COLOR_DIM     0x00909090
#define COLOR_BORDER  0x00606060
#define COLOR_RED     0x00E62117
#define COLOR_BLACK   0x00000000

#define SIDEBAR_WIDTH 125
#define SIDEBAR_SPEED 20

typedef unsigned int u32;

typedef struct {
    int x;
    int y;
    u32 color;
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
    int cbid;

    cbid = sceKernelCreateCallback(
        "Exit Callback",
        exitCallback,
        NULL
    );

    if (cbid >= 0)
        sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();

    return 0;
}

static int SetupCallbacks(void)
{
    int thid;

    thid = sceKernelCreateThread(
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

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        BUF_WIDTH
    );

    sceGuDispBuffer(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        (void *)0x88000,
        BUF_WIDTH
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        BUF_WIDTH
    );

    sceGuOffset(
        2048 - (SCREEN_WIDTH / 2),
        2048 - (SCREEN_HEIGHT / 2)
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuDepthRange(65535, 0);

    sceGuScissor(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuEnable(GU_BLEND);

    sceGuBlendFunc(
        GU_ADD,
        GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA,
        0,
        0
    );

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

static void drawRect(
    int x,
    int y,
    int w,
    int h,
    u32 color
)
{
    Vertex *v;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    v[0].x = x;
    v[0].y = y;
    v[0].color = color;

    v[1].x = x + w;
    v[1].y = y + h;
    v[1].color = color;

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

static void drawBorder(
    int x,
    int y,
    int w,
    int h,
    int thickness,
    u32 color
)
{
    drawRect(
        x,
        y,
        w,
        thickness,
        color
    );

    drawRect(
        x,
        y + h - thickness,
        w,
        thickness,
        color
    );

    drawRect(
        x,
        y,
        thickness,
        h,
        color
    );

    drawRect(
        x + w - thickness,
        y,
        thickness,
        h,
        color
    );
}

/*
 * Simple 5x5 font.
 */

static const unsigned char font5x5[36][5] = {
    {0x0E,0x11,0x1F,0x11,0x11},
    {0x1E,0x11,0x1E,0x11,0x1E},
    {0x0F,0x10,0x10,0x10,0x0F},
    {0x1E,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x1E,0x10,0x1F},
    {0x1F,0x10,0x1E,0x10,0x10},
    {0x0F,0x10,0x17,0x11,0x0F},
    {0x11,0x11,0x1F,0x11,0x11},
    {0x1F,0x04,0x04,0x04,0x1F},
    {0x01,0x01,0x01,0x11,0x0E},
    {0x11,0x12,0x1C,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11},
    {0x0E,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x1E,0x10,0x10},
    {0x0E,0x11,0x11,0x15,0x0E},
    {0x1E,0x11,0x1E,0x12,0x11},
    {0x0F,0x10,0x0E,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x15,0x1B,0x11},
    {0x11,0x0A,0x04,0x0A,0x11},
    {0x11,0x0A,0x04,0x04,0x04},
    {0x1F,0x02,0x04,0x08,0x1F},

    {0x0E,0x11,0x13,0x15,0x19},
    {0x04,0x0C,0x04,0x04,0x0E},
    {0x0E,0x11,0x02,0x04,0x1F},
    {0x1F,0x02,0x06,0x01,0x1E},
    {0x02,0x06,0x0A,0x1F,0x02},
    {0x1F,0x10,0x1E,0x01,0x1E},
    {0x06,0x08,0x1E,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x04},
    {0x0E,0x11,0x0E,0x11,0x0E},
    {0x0E,0x11,0x0F,0x01,0x06}
};

static int fontIndex(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';

    if (c >= '0' && c <= '9')
        return 26 + (c - '0');

    return -1;
}

static void drawChar(
    int x,
    int y,
    char c,
    u32 color,
    int scale
)
{
    int i;
    int j;
    int index;

    index = fontIndex(c);

    if (index < 0)
        return;

    if (scale < 1)
        scale = 1;

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {

            if (font5x5[index][i] &
                (1 << (4 - j))) {

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

static void drawText(
    int x,
    int y,
    const char *text,
    u32 color
)
{
    int i = 0;

    while (text[i] != '\0') {

        drawChar(
            x + i * 6,
            y,
            text[i],
            color,
            1
        );

        i++;
    }
}

static void drawTextLarge(
    int x,
    int y,
    const char *text,
    u32 color
)
{
    int i = 0;

    while (text[i] != '\0') {

        drawChar(
            x + i * 9,
            y,
            text[i],
            color,
            2
        );

        i++;
    }
}

typedef struct {
    const char *title;
    const char *channel;
} Video;

static const Video videos[] = {
    {"WELCOME TO PSP YOUTUBE", "PSP CHANNEL"},
    {"LATEST GAMING VIDEOS", "GAMING"},
    {"PSP HOMEBREW NEWS", "PSP DEV"},
    {"RETRO GAMES", "RETRO"},
    {"TECHNOLOGY", "TECH"},
    {"NEW PSP PROJECTS", "HOMEBREW"}
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

static void drawThumbnail(
    int x,
    int y,
    int w,
    int h,
    int selected
)
{
    drawRect(
        x,
        y,
        w,
        h,
        selected ? COLOR_RED : COLOR_PANEL2
    );

    drawRect(
        x + 5,
        y + 5,
        w - 10,
        h - 10,
        COLOR_BLACK
    );

    /*
     * Play symbol.
     */

    drawRect(
        x + w / 2 - 3,
        y + h / 2 - 18,
        6,
        36,
        COLOR_TEXT
    );

    drawRect(
        x + w / 2 + 3,
        y + h / 2 - 12,
        6,
        24,
        COLOR_TEXT
    );

    drawRect(
        x + w / 2 + 9,
        y + h / 2 - 6,
        6,
        12,
        COLOR_TEXT
    );
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

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        45,
        COLOR_PANEL
    );

    drawTextLarge(
        18,
        12,
        "PSP YOUTUBE",
        COLOR_TEXT
    );

    drawText(
        370,
        18,
        title,
        COLOR_DIM
    );

    drawRect(
        0,
        44,
        SCREEN_WIDTH,
        1,
        COLOR_BORDER
    );
}

static void drawVideoCard(
    int index,
    int x,
    int y,
    int w,
    int h,
    int selected
)
{
    drawThumbnail(
        x,
        y,
        w,
        h,
        selected
    );

    drawText(
        x,
        y + h + 7,
        videos[index].title,
        COLOR_TEXT
    );

    drawText(
        x,
        y + h + 17,
        videos[index].channel,
        COLOR_DIM
    );
}

static void drawHome(int selectedVideo)
{
    int x1 = 20;
    int x2 = 250;
    int y1 = 62;
    int y2 = 167;

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_HOME);

    drawVideoCard(
        0,
        x1,
        y1,
        205,
        88,
        selectedVideo == 0
    );

    drawVideoCard(
        1,
        x2,
        y1,
        205,
        88,
        selectedVideo == 1
    );

    drawVideoCard(
        2,
        x1,
        y2,
        205,
        88,
        selectedVideo == 2
    );

    drawVideoCard(
        3,
        x2,
        y2,
        205,
        88,
        selectedVideo == 3
    );
}

static void drawTrending(int selectedVideo)
{
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_TRENDING);

    drawVideoCard(
        4,
        20,
        62,
        205,
        88,
        selectedVideo == 4
    );

    drawVideoCard(
        5,
        250,
        62,
        205,
        88,
        selectedVideo == 5
    );

    drawVideoCard(
        1,
        20,
        167,
        205,
        88,
        selectedVideo == 1
    );

    drawVideoCard(
        2,
        250,
        167,
        205,
        88,
        selectedVideo == 2
    );
}
static void drawSearch(void)
{
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_SEARCH);

    drawBorder(
        35,
        70,
        410,
        42,
        2,
        COLOR_BORDER
    );

    drawText(
        52,
        87,
        "SEARCH YOUTUBE",
        COLOR_DIM
    );

    drawRect(
        35,
        130,
        125,
        32,
        COLOR_RED
    );

    drawText(
        67,
        142,
        "SEARCH",
        COLOR_TEXT
    );

    drawText(
        35,
        190,
        "PRESS X TO START SEARCH",
        COLOR_DIM
    );
}

static void drawLibrary(void)
{
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_LIBRARY);

    drawTextLarge(
        30,
        80,
        "LIBRARY",
        COLOR_TEXT
    );

    drawText(
        30,
        120,
        "YOUR VIDEOS WILL APPEAR HERE",
        COLOR_DIM
    );

    drawText(
        30,
        150,
        "NETWORK FEATURES COMING LATER",
        COLOR_DIM
    );
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

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_SETTINGS);

    for (i = 0; i < 4; i++) {

        u32 c;

        c = (i == selectedSetting)
            ? COLOR_RED
            : COLOR_PANEL;

        drawRect(
            30,
            65 + i * 42,
            420,
            34,
            c
        );

        drawText(
            48,
            78 + i * 42,
            items[i],
            (i == selectedSetting)
                ? COLOR_TEXT
                : COLOR_DIM
        );
    }

    drawText(
        30,
        245,
        "O BACK",
        COLOR_DIM
    );
}

static void drawVideoDetails(int selectedVideo)
{
    if (selectedVideo < 0)
        selectedVideo = 0;

    if (selectedVideo >= VIDEO_COUNT)
        selectedVideo = VIDEO_COUNT - 1;

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_VIDEO);

    drawThumbnail(
        20,
        60,
        250,
        120,
        0
    );

    drawTextLarge(
        290,
        65,
        "VIDEO",
        COLOR_TEXT
    );

    drawText(
        290,
        95,
        videos[selectedVideo].title,
        COLOR_TEXT
    );

    drawText(
        290,
        115,
        videos[selectedVideo].channel,
        COLOR_DIM
    );

    drawRect(
        290,
        140,
        120,
        32,
        COLOR_RED
    );

    drawText(
        325,
        152,
        "PLAY",
        COLOR_TEXT
    );

    drawBorder(
        20,
        195,
        440,
        55,
        1,
        COLOR_BORDER
    );

    drawText(
        30,
        207,
        "VIDEO DETAILS",
        COLOR_TEXT
    );

    drawText(
        30,
        228,
        "PLAYBACK WILL BE ADDED LATER",
        COLOR_DIM
    );
}

static void drawSidebar(
    int x,
    int selected
)
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

    drawRect(
        x,
        0,
        SIDEBAR_WIDTH,
        SCREEN_HEIGHT,
        COLOR_PANEL
    );

    drawRect(
        x + SIDEBAR_WIDTH - 1,
        0,
        1,
        SCREEN_HEIGHT,
        COLOR_BORDER
    );

    drawTextLarge(
        x + 15,
        18,
        "MENU",
        COLOR_TEXT
    );

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
            i == selected
                ? COLOR_TEXT
                : COLOR_DIM
        );
    }

    drawText(
        x + 18,
        245,
        "L CLOSE",
        COLOR_DIM
    );
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

    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_DIGITAL
    );

    while (running) {

        unsigned int pressed;

        sceCtrlReadBufferPositive(
            &pad,
            1
        );

        pressed =
            pad.Buttons &
            ~oldButtons;

        oldButtons = pad.Buttons;

        /*
         * L opens/closes the sidebar.
         */

        if (pressed & PSP_CTRL_LTRIGGER) {

            sidebarOpen = !sidebarOpen;

            sidebarTarget =
                sidebarOpen
                ? 0
                : -SIDEBAR_WIDTH;
        }

        /*
         * Sidebar controls.
         */

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

                sidebarTarget =
                    -SIDEBAR_WIDTH;
            }

            if (pressed & PSP_CTRL_CIRCLE) {

                sidebarOpen = 0;

                sidebarTarget =
                    -SIDEBAR_WIDTH;
            }
        }
        else {

            /*
             * Home / Trending navigation.
             */

            if (
                page == PAGE_HOME ||
                page == PAGE_TRENDING
            ) {

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

                if (pressed & PSP_CTRL_CROSS) {

                    page = PAGE_VIDEO;
                }
            }

            /*
             * Settings.
             */

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

            /*
             * Search placeholder.
             */

            else if (page == PAGE_SEARCH) {

                if (pressed & PSP_CTRL_CROSS) {
                    /*
                     * Real search will be added later.
                     */
                }
            }

            /*
             * Video placeholder.
             */

            else if (page == PAGE_VIDEO) {

                if (pressed & PSP_CTRL_CROSS) {
                    /*
                     * Real playback will be added later.
                     */
                }
            }

            /*
             * Circle = back.
             */

            if (pressed & PSP_CTRL_CIRCLE) {

                if (page == PAGE_VIDEO)
                    page = PAGE_HOME;
                else
                    page = PAGE_HOME;
            }
        }

        /*
         * Sidebar animation.
         */

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

        /*
         * Rendering.
         */

        sceGuStart(
            GU_DIRECT,
            list
        );

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

        /*
         * Sidebar is rendered last,
         * so it overlays the page.
         */

        drawSidebar(
            sidebarX,
            sidebarSelected
        );

        sceGuFinish();

        sceGuSync(
            0,
            0
        );

        sceDisplayWaitVblankStart();

        sceGuSwapBuffers();

        sceKernelDelayThread(1000);
    }

    sceGuTerm();

    sceKernelExitGame();

    return 0;
}