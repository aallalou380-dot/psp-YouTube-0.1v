#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <psputility.h>
#include <intraFont.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("PSP YouTube 0.2v", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512

static unsigned int __attribute__((aligned(16))) guList[262144];
static volatile int running = 1;
static intraFont *font = 0;
static unsigned int oldButtons = 0;

/* PSP GU colors are AABBGGRR. */
static const unsigned int C_BG       = 0xFF111318;
static const unsigned int C_PANEL    = 0xFF1A1E26;
static const unsigned int C_PANEL2   = 0xFF222731;
static const unsigned int C_PANEL3   = 0xFF292F3A;
static const unsigned int C_TEXT     = 0xFFF4F5F7;
static const unsigned int C_MUTED    = 0xFFA8ADB8;
static const unsigned int C_DIM      = 0xFF6F7580;
static const unsigned int C_RED      = 0xFFFF1238;
static const unsigned int C_RED_DARK = 0xFFB80025;
static const unsigned int C_WHITE    = 0xFFFFFFFF;
static const unsigned int C_LINE     = 0xFF353B47;
static const unsigned int C_BLACK    = 0xFF08090C;

struct Vertex2D {
    unsigned int color;
    short x, y, z;
} __attribute__((packed));

static int page = 0;              /* 0 = home, 1 = video */
static int sidebarOpen = 0;
static int menuIndex = 0;
static int selectedCard = 0;
static char searchText[64] = "";

static const char *menuItems[] = {
    "Home", "Trending", "Shorts", "Library", "Settings"
};

static const char *titles[] = {
    "Discover new videos",
    "PSP homebrew highlights",
    "Gaming Shorts",
    "Your saved videos"
};

static const char *channels[] = {
    "PSP YouTube",
    "PSP Community",
    "Gaming Hub",
    "Library"
};

static const char *viewCounts[] = {
    "12K views", "8.4K views", "4.1K views", "1.7K views"
};

static int exitCallback(int, int, void *)
{
    running = 0;
    sceKernelExitGame();
    return 0;
}

static int callbackThread(SceSize, void *)
{
    SceUID cb = sceKernelCreateCallback("ExitCallback", exitCallback, 0);
    if (cb >= 0)
        sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupCallbacks()
{
    SceUID thid = sceKernelCreateThread("CallbackThread", callbackThread,
                                        0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
}

/*
 * intraFont changes GU texture/blend state internally. Resetting the state
 * after every text call is important when mixing intraFont with GU sprites.
 * Without this, later rectangles/lines can become textured or corrupted.
 */
static void reset2DState()
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuShadeModel(GU_FLAT);
    sceGuColor(C_WHITE);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
}

static void fillRect(int x, int y, int w, int h, unsigned int color)
{
    if (w <= 0 || h <= 0)
        return;

    reset2DState();

    Vertex2D *v = (Vertex2D *)sceGuGetMemory(2 * sizeof(Vertex2D));
    if (!v)
        return;

    v[0].color = color;
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   2, 0, v);
}

static void drawBorder(int x, int y, int w, int h, unsigned int color, int thickness)
{
    if (thickness < 1)
        thickness = 1;

    fillRect(x, y, w, thickness, color);
    fillRect(x, y + h - thickness, w, thickness, color);
    fillRect(x, y, thickness, h, color);
    fillRect(x + w - thickness, y, thickness, h, color);
}

static void drawLine(int x1, int y1, int x2, int y2, unsigned int color)
{
    reset2DState();

    Vertex2D *v = (Vertex2D *)sceGuGetMemory(2 * sizeof(Vertex2D));
    if (!v)
        return;

    v[0].color = color;
    v[0].x = (short)x1;
    v[0].y = (short)y1;
    v[0].z = 0;
    v[1].color = color;
    v[1].x = (short)x2;
    v[1].y = (short)y2;
    v[1].z = 0;

    sceGuDrawArray(GU_LINES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   2, 0, v);
}

static void drawText(const char *str, float x, float y,
                     unsigned int color, float scale)
{
    if (!font || !str || !str[0])
        return;

    intraFontSetStyle(font, scale, color, 0x00000000, 0,
                      INTRAFONT_ALIGN_LEFT);
    intraFontPrint(font, x, y, str);

    /* Critical: intraFont changes texture state. */
    reset2DState();
}

static void drawPlayTriangle(int cx, int cy, int size, unsigned int color)
{
    reset2DState();

    Vertex2D *v = (Vertex2D *)sceGuGetMemory(3 * sizeof(Vertex2D));
    if (!v)
        return;

    v[0].color = color;
    v[0].x = (short)(cx - size / 2);
    v[0].y = (short)(cy - size);
    v[0].z = 0;

    v[1].color = color;
    v[1].x = (short)(cx - size / 2);
    v[1].y = (short)(cy + size);
    v[1].z = 0;

    v[2].color = color;
    v[2].x = (short)(cx + size);
    v[2].y = (short)cy;
    v[2].z = 0;

    sceGuDrawArray(GU_TRIANGLES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   3, 0, v);
}

static void drawYouTubeLogo(int x, int y)
{
    /* Layered rectangles give a clean rounded-ish YouTube mark at PSP scale. */
    fillRect(x + 3, y, 42, 30, C_RED);
    fillRect(x, y + 5, 48, 20, C_RED);
    drawPlayTriangle(x + 25, y + 15, 8, C_WHITE);
}

static void drawSearchIcon(int x, int y)
{
    reset2DState();
    drawLine(x, y, x + 10, y + 10, C_MUTED);
    fillRect(x + 9, y + 9, 3, 3, C_MUTED);
}

static void drawHeader()
{
    drawYouTubeLogo(17, 15);
    drawText("YouTube", 73, 36, C_TEXT, 0.70f);

    fillRect(130, 13, 265, 35, C_PANEL2);
    drawBorder(130, 13, 265, 35, C_LINE, 1);
    drawSearchIcon(143, 22);

    if (searchText[0])
        drawText(searchText, 166, 36, C_TEXT, 0.52f);
    else
        drawText("Search YouTube", 166, 36, C_MUTED, 0.52f);

    fillRect(405, 13, 42, 35, C_RED);
    drawText("R", 420, 36, C_WHITE, 0.60f);
}

static void drawSidebar()
{
    if (!sidebarOpen)
        return;

    fillRect(0, 0, 177, SCREEN_H, 0xFF151820);
    fillRect(0, 0, 177, 58, C_PANEL);
    drawLine(176, 0, 176, SCREEN_H, C_LINE);

    drawYouTubeLogo(14, 14);
    drawText("YouTube", 68, 36, C_TEXT, 0.63f);

    for (int i = 0; i < 5; ++i) {
        int y = 72 + i * 37;

        if (i == menuIndex) {
            fillRect(11, y - 23, 154, 31, C_RED);
            drawText(menuItems[i], 29, y, C_WHITE, 0.54f);
        } else {
            drawText(menuItems[i], 29, y, C_MUTED, 0.54f);
        }
    }

    drawText("L  close", 104, 255, C_DIM, 0.38f);
}

static void drawThumbnail(int x, int y, int w, int h, int index)
{
    static const unsigned int colors[4] = {
        0xFF28313F, 0xFF263B3C, 0xFF3B2E42, 0xFF293B2D
    };

    fillRect(x, y, w, h, colors[index & 3]);
    fillRect(x + 7, y + 7, w - 14, 3, 0xFF46505E);
    fillRect(x + 10, y + h - 17, w - 20, 4, 0xFF3E4754);

    /* Decorative video frame shapes. */
    fillRect(x + 14, y + 18, 42, 20, 0xFF313B4A);
    fillRect(x + 62, y + 18, w - 76, 7, 0xFF46505E);
    fillRect(x + 62, y + 30, w - 88, 6, 0xFF3E4754);

    drawPlayTriangle(x + w / 2, y + h / 2, 13, C_RED);
}

static void drawVideoCard(int index, int x, int y)
{
    const int w = 138;
    const int h = 105;

    fillRect(x, y, w, h, C_PANEL);

    if (index == selectedCard)
        drawBorder(x - 2, y - 2, w + 4, h + 4, C_RED, 2);
    else
        drawBorder(x, y, w, h, C_LINE, 1);

    drawThumbnail(x + 7, y + 7, w - 14, 57, index);
    drawText(titles[index], x + 8, y + 80, C_TEXT, 0.40f);
    drawText(channels[index], x + 8, y + 94, C_MUTED, 0.34f);
    drawText(viewCounts[index], x + 77, y + 94, C_DIM, 0.31f);
}

static void drawHomeContent()
{
    drawHeader();

    const char *heading = searchText[0] ? "Search results" : "Recommended";
    drawText(heading, 18, 70, C_TEXT, 0.64f);

    drawVideoCard(0, 18, 81);
    drawVideoCard(1, 170, 81);
    drawVideoCard(2, 322, 81);
    drawVideoCard(3, 18, 197);

    fillRect(170, 197, 290, 45, C_PANEL);
    drawBorder(170, 197, 290, 45, C_LINE, 1);
    drawText("X", 184, 224, C_RED, 0.52f);
    drawText("Open selected video", 207, 224, C_MUTED, 0.44f);

    drawText("L  Menu", 397, 266, C_DIM, 0.34f);

    drawSidebar();
}

static void drawVideoPage()
{
    fillRect(0, 0, SCREEN_W, SCREEN_H, C_BG);

    fillRect(18, 13, 444, 125, C_BLACK);
    drawBorder(18, 13, 444, 125, C_LINE, 1);
    fillRect(29, 24, 422, 103, 0xFF1C212A);

    drawPlayTriangle(240, 75, 21, C_RED);

    drawText(titles[selectedCard], 20, 161, C_TEXT, 0.70f);
    drawText(channels[selectedCard], 20, 184, C_MUTED, 0.49f);
    drawText(viewCounts[selectedCard], 20, 202, C_DIM, 0.40f);

    fillRect(18, 214, 444, 1, C_LINE);
    drawText("About this video", 20, 235, C_TEXT, 0.50f);
    drawText("A modern PSP YouTube interface preview.", 20, 254, C_MUTED, 0.40f);
    drawText("O  Back", 414, 267, C_DIM, 0.34f);
}

/*
 * PSP system OSK. The dialog is modal: the render loop pauses while the
 * official PSP utility keyboard is active. This keeps the normal UI stable.
 */
static int openSearchOSK()
{
    enum { MAX_CHARS = 64 };

    static unsigned short inputText[MAX_CHARS];
    static unsigned short outputText[MAX_CHARS];
    static unsigned short description[MAX_CHARS];
    static SceUtilityOskData data;
    static SceUtilityOskParams params;

    memset(inputText, 0, sizeof(inputText));
    memset(outputText, 0, sizeof(outputText));
    memset(description, 0, sizeof(description));
    memset(&data, 0, sizeof(data));
    memset(&params, 0, sizeof(params));

    int i;
    for (i = 0; searchText[i] && i < MAX_CHARS - 1; ++i)
        inputText[i] = (unsigned char)searchText[i];

    const char *label = "Search";
    for (i = 0; label[i] && i < MAX_CHARS - 1; ++i)
        description[i] = (unsigned char)label[i];

    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = description;
    data.intext = inputText;
    data.outtextlength = MAX_CHARS;
    data.outtextlimit = MAX_CHARS - 1;
    data.outtext = outputText;

    params.base.size = sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                                &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,
                                &params.base.buttonSwap);

    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    if (sceUtilityOskInitStart(&params) < 0)
        return 0;

    int done = 0;
    while (!done && running) {
        int status = sceUtilityOskGetStatus();

        if (status == PSP_UTILITY_DIALOG_VISIBLE) {
            sceUtilityOskUpdate(1);
        } else if (status == PSP_UTILITY_DIALOG_QUIT) {
            sceUtilityOskShutdownStart();
        } else if (status == PSP_UTILITY_DIALOG_NONE) {
            done = 1;
        }

        sceDisplayWaitVblankStart();
    }

    if (data.result == PSP_UTILITY_OSK_RESULT_CHANGED) {
        int n = 0;
        while (n < MAX_CHARS - 1 && outputText[n]) {
            unsigned short ch = outputText[n];
            searchText[n] = (ch < 128) ? (char)ch : '?';
            ++n;
        }
        searchText[n] = '\0';
    }

    return 1;
}

static void handleInput()
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    unsigned int buttons = pad.Buttons;
    unsigned int pressed = buttons & ~oldButtons;

    /* Sidebar */
    if (pressed & PSP_CTRL_LTRIGGER) {
        sidebarOpen = !sidebarOpen;
        oldButtons = buttons;
        return;
    }

    if (sidebarOpen) {
        if (pressed & PSP_CTRL_UP)
            menuIndex = (menuIndex + 4) % 5;
        if (pressed & PSP_CTRL_DOWN)
            menuIndex = (menuIndex + 1) % 5;

        if (pressed & PSP_CTRL_CROSS) {
            /* Home returns to the main page. Other entries currently act as
             * visual sections until the network/content layer is added. */
            page = 0;
            sidebarOpen = 0;
        }

        oldButtons = buttons;
        return;
    }

    if (page == 0) {
        if (pressed & PSP_CTRL_RTRIGGER) {
            openSearchOSK();
        }

        if (pressed & PSP_CTRL_LEFT)
            selectedCard = (selectedCard + 3) % 4;
        if (pressed & PSP_CTRL_RIGHT)
            selectedCard = (selectedCard + 1) % 4;
        if (pressed & PSP_CTRL_UP)
            selectedCard = (selectedCard + 2) % 4;
        if (pressed & PSP_CTRL_DOWN)
            selectedCard = (selectedCard + 2) % 4;

        if (pressed & PSP_CTRL_CROSS)
            page = 1;
    } else {
        if (pressed & PSP_CTRL_CIRCLE)
            page = 0;
    }

    oldButtons = buttons;
}

static void initGU()
{
    sceGuInit();

    void *fb0 = guGetStaticVramBuffer(BUFFER_W, SCREEN_H, GU_PSM_8888);
    void *fb1 = guGetStaticVramBuffer(BUFFER_W, SCREEN_H, GU_PSM_8888);
    void *zb  = guGetStaticVramBuffer(BUFFER_W, SCREEN_H, GU_PSM_4444);

    sceGuStart(GU_DIRECT, guList);

    sceGuDrawBuffer(GU_PSM_8888, fb0, BUFFER_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, fb1, BUFFER_W);
    sceGuDepthBuffer(zb, BUFFER_W);

    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuShadeModel(GU_FLAT);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void renderFrame()
{
    sceGuStart(GU_DIRECT, guList);

    reset2DState();
    sceGuClearColor(C_BG);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    if (page == 0)
        drawHomeContent();
    else
        drawVideoPage();

    /* intraFont may change texture state near the end of a frame. */
    reset2DState();

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main()
{
    setupCallbacks();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    initGU();

    /* If the firmware font cannot be loaded, the graphical UI still runs. */
    font = intraFontLoad("flash0:/font/ltn0.pgf", INTRAFONT_CACHE_ALL);

    while (running) {
        handleInput();
        renderFrame();
    }

    if (font)
        intraFontUnload(font);

    sceGuTerm();
    return 0;
}
