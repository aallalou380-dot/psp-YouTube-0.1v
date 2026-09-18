#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <intraFont.h>

#include <stdint.h>

PSP_MODULE_INFO("PSP YouTube 0.2v", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512

static unsigned int __attribute__((aligned(16))) displayList[262144];

static volatile int running = 1;

struct Vertex
{
    float u, v;
    unsigned int color;
    float x, y, z;
};

static unsigned int color(
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a = 255)
{
    return ((unsigned int)a << 24) |
           ((unsigned int)b << 16) |
           ((unsigned int)g << 8) |
           r;
}

/* ---------------------------------------------------------
   Exit callback
   --------------------------------------------------------- */

static int exitCallback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;

    running = 0;
    return 0;
}

static int callbackThread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    int callbackId =
        sceKernelCreateCallback(
            "Exit Callback",
            exitCallback,
            NULL);

    if (callbackId >= 0)
    {
        sceKernelRegisterExitCallback(callbackId);
    }

    sceKernelSleepThreadCB();

    return 0;
}

static int setupCallbacks()
{
    int thread =
        sceKernelCreateThread(
            "Callback Thread",
            callbackThread,
            0x11,
            0xFA0,
            0,
            NULL);

    if (thread >= 0)
    {
        sceKernelStartThread(thread, 0, NULL);
    }

    return thread;
}

/* ---------------------------------------------------------
   GU
   --------------------------------------------------------- */

static void initGU()
{
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        BUFFER_W);

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        BUFFER_W);

    sceGuDepthBuffer(
        (void *)0x110000,
        BUFFER_W);

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2);

    sceGuViewport(
        2048,
        2048,
        SCREEN_W,
        SCREEN_H);

    sceGuScissor(
        0,
        0,
        SCREEN_W,
        SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);

    sceGuFinish();
    sceGuSync(0, 0);

    sceGuDisplay(GU_TRUE);
}

static void beginFrame()
{
    sceGuStart(GU_DIRECT, displayList);

    sceGuClearColor(
        color(10, 12, 17));

    sceGuClear(
        GU_COLOR_BUFFER_BIT);
}

static void endFrame()
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();

    sceGuSwapBuffers();
}

/* ---------------------------------------------------------
   Basic graphic primitives
   --------------------------------------------------------- */

static void drawRect(
    float x,
    float y,
    float w,
    float h,
    unsigned int c)
{
    Vertex *v =
        (Vertex *)sceGuGetMemory(
            sizeof(Vertex) * 2);

    v[0].u = 0;
    v[0].v = 0;
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = 0;
    v[1].v = 0;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        NULL,
        v);
}

/* ---------------------------------------------------------
   Generated thumbnail texture
   --------------------------------------------------------- */

static uint32_t texture0[128 * 64]
    __attribute__((aligned(16)));

static uint32_t texture1[128 * 64]
    __attribute__((aligned(16)));

static uint32_t texture2[128 * 64]
    __attribute__((aligned(16)));

static void createTexture(
    uint32_t *buffer,
    int w,
    int h,
    int r1,
    int g1,
    int b1,
    int r2,
    int g2,
    int b2)
{
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float t =
                (float)y /
                (float)(h - 1);

            int r =
                (int)(r1 * (1.0f - t) + r2 * t);

            int g =
                (int)(g1 * (1.0f - t) + g2 * t);

            int b =
                (int)(b1 * (1.0f - t) + b2 * t);

            /*
             * Subtle graphical pattern.
             * This is only a temporary test texture.
             */

            if (((x / 8) + (y / 8)) % 2 == 0)
            {
                r += 5;
                g += 5;
                b += 5;
            }

            if (x > 82 && y < 24)
            {
                r = 235;
                g = 35;
                b = 55;
            }

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            buffer[y * w + x] =
                color(
                    (unsigned char)r,
                    (unsigned char)g,
                    (unsigned char)b);
        }
    }

    sceKernelDcacheWritebackInvalidateAll();
}

static void drawTexture(
    uint32_t *texture,
    int textureWidth,
    int textureHeight,
    float x,
    float y,
    float w,
    float h)
{
    Vertex *v =
        (Vertex *)sceGuGetMemory(
            sizeof(Vertex) * 2);

    v[0].u = 0;
    v[0].v = 0;
    v[0].color = color(255, 255, 255);
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = (float)textureWidth;
    v[1].v = (float)textureHeight;
    v[1].color = color(255, 255, 255);
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuTexMode(
        GU_PSM_8888,
        0,
        0,
        GU_FALSE);

    sceGuTexFunc(
        GU_TFX_REPLACE,
        GU_TCC_RGBA);

    sceGuTexFilter(
        GU_LINEAR,
        GU_LINEAR);

    sceGuTexWrap(
        GU_CLAMP,
        GU_CLAMP);

    sceGuTexImage(
        0,
        textureWidth,
        textureHeight,
        textureWidth,
        texture);

    sceGuEnable(GU_TEXTURE_2D);

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        NULL,
        v);

    sceGuDisable(GU_TEXTURE_2D);
}

/* ---------------------------------------------------------
   Text
   --------------------------------------------------------- */

static intraFont *font = NULL;

static void drawText(
    float x,
    float y,
    const char *str,
    unsigned int c,
    float scale)
{
    if (!font)
        return;

    intraFontSetStyle(
        font,
        scale,
        scale,
        c,
        0.0f,
        INTRAFONT_ALIGN_LEFT);

    intraFontPrint(
        font,
        x,
        y,
        str);
}

/* ---------------------------------------------------------
   Header
   --------------------------------------------------------- */

static void drawHeader()
{
    drawRect(
        116,
        0,
        SCREEN_W - 116,
        48,
        color(13, 16, 22));

    drawRect(
        116,
        47,
        SCREEN_W - 116,
        1,
        color(42, 45, 53));

    drawText(
        136,
        31,
        "YouTube",
        color(255, 255, 255),
        0.90f);

    /*
     * Search area.
     */

    drawRect(
        323,
        10,
        130,
        28,
        color(28, 32, 40));

    drawText(
        337,
        29,
        "Search",
        color(140, 145, 155),
        0.60f);
}

/* ---------------------------------------------------------
   Sidebar
   --------------------------------------------------------- */

static void drawSidebar(
    int selected,
    bool focus)
{
    drawRect(
        0,
        0,
        116,
        SCREEN_H,
        color(15, 18, 25));

    drawRect(
        0,
        0,
        116,
        2,
        color(235, 35, 55));

    drawText(
        20,
        31,
        "PSP",
        color(255, 255, 255),
        0.90f);

    const char *items[5] =
    {
        "Home",
        "Trending",
        "Shorts",
        "Library",
        "Settings"
    };

    for (int i = 0; i < 5; ++i)
    {
        float y =
            65.0f + i * 36.0f;

        if (i == selected)
        {
            drawRect(
                9,
                y - 20,
                98,
                29,
                focus
                    ? color(48, 53, 65)
                    : color(36, 40, 49));

            drawRect(
                9,
                y - 20,
                3,
                29,
                color(235, 35, 55));
        }

        drawText(
            23,
            y,
            items[i],
            i == selected
                ? color(255, 255, 255)
                : color(145, 150, 160),
            0.64f);
    }
}

/* ---------------------------------------------------------
   Video cards
   --------------------------------------------------------- */

static void drawVideoCard(
    int index,
    bool selected,
    uint32_t *texture)
{
    const float x =
        130.0f + index * 108.0f;

    const float y = 72.0f;

    const float w = 101.0f;
    const float h = 122.0f;

    /*
     * Selected card.
     */

    if (selected)
    {
        drawRect(
            x - 2,
            y - 2,
            w + 4,
            h + 4,
            color(235, 35, 55));
    }

    drawRect(
        x,
        y,
        w,
        h,
        color(24, 28, 36));

    drawTexture(
        texture,
        128,
        64,
        x + 3,
        y + 3,
        w - 6,
        54);

    const char *title[3] =
    {
        "PSP News",
        "Homebrew",
        "Gaming"
    };

    const char *channel[3] =
    {
        "PSP Channel",
        "PSP Dev",
        "Gaming Lab"
    };

    drawText(
        x + 6,
        y + 74,
        title[index],
        color(242, 243, 246),
        0.51f);

    drawText(
        x + 6,
        y + 90,
        channel[index],
        color(145, 150, 160),
        0.43f);

    drawText(
        x + 6,
        y + 106,
        "12K views",
        color(112, 118, 128),
        0.40f);

    drawText(
        x + 6,
        y + 120,
        "8:24",
        color(112, 118, 128),
        0.40f);
}

/* ---------------------------------------------------------
   Input
   --------------------------------------------------------- */

static void updateInput(
    int &card,
    int &sidebar,
    bool &sidebarFocus)
{
    static SceCtrlData pad;
    static unsigned int oldButtons = 0;

    sceCtrlReadBufferPositive(
        &pad,
        1);

    unsigned int pressed =
        pad.Buttons & ~oldButtons;

    oldButtons =
        pad.Buttons;

    if (pressed & PSP_CTRL_HOME)
    {
        running = 0;
        return;
    }

    if (pressed & PSP_CTRL_LTRIGGER)
    {
        sidebarFocus = true;
    }

    if (pressed & PSP_CTRL_RTRIGGER)
    {
        sidebarFocus = false;
    }

    if (sidebarFocus)
    {
        if (pressed & PSP_CTRL_UP)
        {
            --sidebar;

            if (sidebar < 0)
                sidebar = 4;
        }

        if (pressed & PSP_CTRL_DOWN)
        {
            ++sidebar;

            if (sidebar > 4)
                sidebar = 0;
        }

        if (pressed & PSP_CTRL_RIGHT)
        {
            sidebarFocus = false;
        }
    }
    else
    {
        if (pressed & PSP_CTRL_LEFT)
        {
            --card;

            if (card < 0)
                card = 2;
        }

        if (pressed & PSP_CTRL_RIGHT)
        {
            ++card;

            if (card > 2)
                card = 0;
        }

        if (pressed & PSP_CTRL_LTRIGGER)
        {
            sidebarFocus = true;
        }
    }
}

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main()
{
    setupCallbacks();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_ANALOG);

    initGU();

    /*
     * PSP's system font.
     * intraFont is used only for text.
     */

    font =
        intraFontLoad(
            "flash0:/font/ltn0.pgf",
            INTRAFONT_CACHE_MED);

    /*
     * Temporary GPU textures.
     */

    createTexture(
        texture0,
        128,
        64,
        48, 82, 130,
        17, 27, 48);

    createTexture(
        texture1,
        128,
        64,
        82, 48, 100,
        26, 18, 44);

    createTexture(
        texture2,
        128,
        64,
        42, 105, 80,
        15, 37, 32);

    int card = 0;
    int sidebar = 0;

    bool sidebarFocus = false;

    while (running)
    {
        updateInput(
            card,
            sidebar,
            sidebarFocus);

        beginFrame();

        drawSidebar(
            sidebar,
            sidebarFocus);

        drawHeader();

        drawText(
            130,
            64,
            "Recommended",
            color(190, 194, 202),
            0.55f);

        drawVideoCard(
            0,
            !sidebarFocus && card == 0,
            texture0);

        drawVideoCard(
            1,
            !sidebarFocus && card == 1,
            texture1);

        drawVideoCard(
            2,
            !sidebarFocus && card == 2,
            texture2);

        /*
         * Bottom navigation hint.
         */

        drawRect(
            130,
            218,
            325,
            35,
            color(17, 20, 27));

        drawText(
            142,
            240,
            "D-pad Navigate",
            color(130, 135, 145),
            0.45f);

        drawText(
            270,
            240,
            "L Menu",
            color(130, 135, 145),
            0.45f);

        drawText(
            350,
            240,
            "R Content",
            color(130, 135, 145),
            0.45f);

        endFrame();
    }

    if (font)
        intraFontUnload(font);

    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    sceKernelExitGame();

    return 0;
}
