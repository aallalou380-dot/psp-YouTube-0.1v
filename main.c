#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <psputility.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <intraFont.h>

PSP_MODULE_INFO("PSP YouTube 0.2v", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  272
#define BUF_WIDTH      512
#define SIDEBAR_WIDTH  150
#define SEARCH_MAX     63
#define OSK_MAX        64

/* PSP GU colors are stored as ABGR. */
#define COLOR_BG       0xFF0A0C10
#define COLOR_BG2      0xFF10141A
#define COLOR_PANEL    0xFF171C23
#define COLOR_PANEL2   0xFF202731
#define COLOR_PANEL3   0xFF262E39
#define COLOR_LINE     0xFF36404C
#define COLOR_TEXT     0xFFF7F8FA
#define COLOR_MUTED    0xFF9DA6B2
#define COLOR_DIM      0xFF687381
#define COLOR_RED      0xFF0000FF
#define COLOR_RED_2    0xFF1717D6
#define COLOR_RED_SOFT 0xA00000FF
#define COLOR_WHITE    0xFFFFFFFF
#define COLOR_BLACK    0xFF000000
#define COLOR_GREEN    0xFF64B58A

static unsigned int __attribute__((aligned(16))) list[262144];
static volatile int running = 1;
static intraFont *uiFont = NULL;
static int fontReady = 0;
static unsigned int frameCounter = 0;
static char searchQuery[SEARCH_MAX + 1] = "";

/* ------------------------------------------------------------- */
/* Data                                                          */
/* ------------------------------------------------------------- */

typedef struct {
    const char *title;
    const char *cardTitle1;
    const char *cardTitle2;
    const char *channel;
    const char *views;
    const char *duration;
    const char *description;
    int style;
} Video;

static const Video videos[] = {
    {"GTA VCS on PSP - Best Moments", "GTA VCS on PSP", "Best Moments", "ZetaGamez", "2.4M views", "12:34",
     "A compact showcase of gameplay, tricks and memorable moments.", 0},
    {"Spider-Man 2 - Amazing Gameplay", "Spider-Man 2", "Amazing Gameplay", "PlayStation", "5.1M views", "15:42",
     "Highlights, movement and action from a modern superhero adventure.", 1},
    {"Beautiful Places - Relaxing 4K", "Beautiful Places", "Relaxing 4K", "Nature Relaxation", "1.8M views", "10:28",
     "Calm landscapes and atmospheric scenery for a relaxing break.", 2},
    {"Forza Horizon - Fastest Drives", "Forza Horizon", "Fastest Drives", "Xbox", "3.7M views", "18:16",
     "Fast races, open roads and a selection of favorite cars.", 3},
    {"PSP Homebrew News", "PSP Homebrew", "News", "PSP Dev", "640K views", "04:46",
     "A roundup of recent PSP homebrew projects, updates and releases.", 4},
    {"New PSP Projects You Should See", "New PSP Projects", "You Should See", "Homebrew", "920K views", "08:16",
     "New software, interfaces and experiments being made for PSP.", 5}
};

#define VIDEO_COUNT ((int)(sizeof(videos) / sizeof(videos[0])))

enum {
    PAGE_HOME = 0,
    PAGE_TRENDING,
    PAGE_SHORTS,
    PAGE_LIBRARY,
    PAGE_SETTINGS,
    PAGE_SEARCH,
    PAGE_VIDEO
};

static const char *sidebarItems[] = {
    "HOME",
    "TRENDING",
    "SHORTS",
    "LIBRARY",
    "SETTINGS"
};

/* ------------------------------------------------------------- */
/* Callbacks / graphics                                          */
/* ------------------------------------------------------------- */

typedef struct {
    u32 color;
    short x;
    short y;
    short z;
} Vertex;

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
    int cbid;
    (void)args;
    (void)argp;

    cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    if (cbid >= 0)
        sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();
    return 0;
}

static int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callbackThread,
                                     0x11, 0xFA0, 0, NULL);
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
    sceGuOffset(2048 - SCREEN_WIDTH / 2, 2048 - SCREEN_HEIGHT / 2);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuClearColor(COLOR_BG);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void initFont(void)
{
    if (!intraFontInit())
        return;

    uiFont = intraFontLoad("flash0:/font/ltn0.pgf", INTRAFONT_CACHE_ASCII);
    if (uiFont) {
        intraFontSetEncoding(uiFont, INTRAFONT_STRING_ASCII);
        fontReady = 1;
    }
}

static void shutdownFont(void)
{
    if (uiFont) {
        intraFontUnload(uiFont);
        uiFont = NULL;
    }
    if (fontReady)
        intraFontShutdown();
    fontReady = 0;
}

/* ------------------------------------------------------------- */
/* Primitive drawing                                             */
/* ------------------------------------------------------------- */

static void drawRect(int x, int y, int w, int h, u32 color)
{
    Vertex *v;
    if (w <= 0 || h <= 0)
        return;

    sceGuDisable(GU_TEXTURE_2D);
    v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
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
                   2, NULL, v);
}

static void drawLine(int x1, int y1, int x2, int y2, int thickness, u32 color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    int i;

    if (thickness < 1)
        thickness = 1;

    if (steps <= 0) {
        drawRect(x1, y1, thickness, thickness, color);
        return;
    }

    for (i = 0; i <= steps; ++i) {
        int x = x1 + dx * i / steps;
        int y = y1 + dy * i / steps;
        drawRect(x, y, thickness, thickness, color);
    }
}

static void drawRoundedPanel(int x, int y, int w, int h, u32 color)
{
    int r = 7;

    if (w < 2 * r + 2 || h < 2 * r + 2) {
        drawRect(x, y, w, h, color);
        return;
    }

    drawRect(x + r, y, w - 2 * r, h, color);
    drawRect(x, y + r, r, h - 2 * r, color);
    drawRect(x + w - r, y + r, r, h - 2 * r, color);
    drawRect(x + 2, y + 2, w - 4, h - 4, color);
}

static void drawRoundedOutline(int x, int y, int w, int h, u32 color)
{
    int r = 7;

    if (w <= 2 || h <= 2)
        return;

    drawRect(x + r, y, w - 2 * r, 1, color);
    drawRect(x + r, y + h - 1, w - 2 * r, 1, color);
    drawRect(x, y + r, 1, h - 2 * r, color);
    drawRect(x + w - 1, y + r, 1, h - 2 * r, color);

    drawRect(x + 3, y + 2, 2, 1, color);
    drawRect(x + 2, y + 3, 1, 2, color);
    drawRect(x + w - 5, y + 2, 2, 1, color);
    drawRect(x + w - 3, y + 3, 1, 2, color);
    drawRect(x + 3, y + h - 3, 2, 1, color);
    drawRect(x + 2, y + h - 5, 1, 2, color);
    drawRect(x + w - 5, y + h - 3, 2, 1, color);
    drawRect(x + w - 3, y + h - 5, 1, 2, color);
}

static void drawText(float x, float y, const char *text, float size, u32 color)
{
    if (!fontReady || !uiFont || !text || !text[0])
        return;

    /* Keep text smooth; all UI text goes through intraFont. */
    intraFontSetStyle(uiFont, size, color, 0x65000000, 0.0f, 0);
    intraFontPrint(uiFont, x, y, text);
    sceGuDisable(GU_TEXTURE_2D);
}

static void drawTextClipped(int x, int y, int w, const char *text, float size, u32 color)
{
    if (!fontReady || !uiFont || !text || !text[0] || w <= 0)
        return;

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(x, y - 16, x + w, y + 6);
    intraFontSetStyle(uiFont, size, color, 0x65000000, 0.0f, 0);
    intraFontPrint(uiFont, (float)x, (float)y, text);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void drawPlayIcon(int cx, int cy, int size, u32 color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(3 * sizeof(Vertex));

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
                   3, NULL, v);
}

static void drawYoutubeLogo(int x, int y, int w, int h)
{
    drawRoundedPanel(x, y, w, h, COLOR_RED);
    drawPlayIcon(x + w / 2 + 1, y + h / 2, h / 3, COLOR_WHITE);
}

static void drawSearchIcon(int cx, int cy, u32 color)
{
    int i;
    double a;

    for (i = 0; i < 18; ++i) {
        a = ((double)i / 18.0) * 6.2831853;
        drawRect(cx - 5 + (int)(5.0 * cos(a)),
                 cy - 5 + (int)(5.0 * sin(a)), 2, 2, color);
    }
    drawLine(cx + 3, cy + 3, cx + 10, cy + 10, 2, color);
}

static void drawMenuIcon(int cx, int cy, u32 color)
{
    drawRect(cx - 8, cy - 7, 16, 2, color);
    drawRect(cx - 8, cy - 1, 16, 2, color);
    drawRect(cx - 8, cy + 5, 16, 2, color);
}

static void drawHomeIcon(int x, int y, u32 color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(3 * sizeof(Vertex));

    v[0].color = color; v[0].x = (short)x;     v[0].y = (short)(y - 7); v[0].z = 0;
    v[1].color = color; v[1].x = (short)(x - 9); v[1].y = (short)(y + 1); v[1].z = 0;
    v[2].color = color; v[2].x = (short)(x + 9); v[2].y = (short)(y + 1); v[2].z = 0;

    sceGuDrawArray(GU_TRIANGLES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   3, NULL, v);
    drawRect(x - 6, y, 12, 9, color);
}

static void drawTrendingIcon(int x, int y, u32 color)
{
    drawRect(x - 2, y - 8, 4, 14, color);
    drawRect(x - 7, y - 3, 5, 9, color);
    drawRect(x + 3, y - 5, 4, 11, color);
    drawRect(x - 8, y + 6, 16, 2, color);
}

static void drawShortsIcon(int x, int y, u32 color)
{
    drawRect(x - 7, y - 8, 5, 16, color);
    drawRect(x + 2, y - 8, 5, 16, color);
    drawRect(x - 2, y - 1, 4, 2, color);
}

static void drawLibraryIcon(int x, int y, u32 color)
{
    drawRect(x - 9, y - 7, 18, 4, color);
    drawRect(x - 7, y - 1, 14, 4, color);
    drawRect(x - 5, y + 5, 10, 4, color);
}

static void drawGearIcon(int x, int y, u32 color)
{
    drawRect(x - 8, y - 2, 16, 5, color);
    drawRect(x - 2, y - 8, 5, 17, color);
    drawRect(x - 6, y - 6, 4, 4, color);
    drawRect(x + 3, y + 3, 4, 4, color);
    drawRect(x - 2, y - 2, 4, 4, COLOR_PANEL);
}

static void drawPauseIcon(int cx, int cy, u32 color)
{
    drawRect(cx - 7, cy - 8, 5, 16, color);
    drawRect(cx + 2, cy - 8, 5, 16, color);
}

/* ------------------------------------------------------------- */
/* Thumbnail artwork                                             */
/* ------------------------------------------------------------- */

static void drawThumbnailArt(int x, int y, int w, int h, int style)
{
    int i;

    if (style == 0) {
        /* Driving / gaming scene */
        drawRect(x, y, w, h / 2, 0xFF2A3558);
        drawRect(x, y + h / 2, w, h / 2, 0xFF172029);
        drawRect(x + 10, y + 18, w - 20, 6, 0xFF785D33);
        drawRect(x + 20, y + 31, w - 40, 28, 0xFF263746);
        drawRect(x + 28, y + 37, w - 56, 16, 0xFF11161B);
        drawRect(x + 36, y + 42, 8, 6, 0xFFC7D1D8);
        drawRect(x + w - 44, y + 42, 8, 6, 0xFFC7D1D8);
        drawRect(x + 8, y + h - 13, w - 16, 3, COLOR_RED);
    } else if (style == 1) {
        /* Superhero / red scene */
        drawRect(x, y, w, h, 0xFF1E2028);
        drawRect(x, y, w, h / 2, 0xFF40202E);
        drawRect(x + 12, y + 12, w - 24, 20, 0xFF6F293D);
        drawRect(x + w / 2 - 8, y + 35, 16, 33, 0xFF3D4E65);
        drawRect(x + w / 2 - 28, y + 45, 18, 9, 0xFFC15C66);
        drawRect(x + w / 2 + 10, y + 41, 18, 9, 0xFFC15C66);
        drawRect(x + 9, y + h - 12, w - 18, 3, 0xFFB83955);
    } else if (style == 2) {
        /* Nature scene */
        drawRect(x, y, w, h / 2, 0xFF6E8DAD);
        drawRect(x, y + h / 2, w, h / 2, 0xFF263E30);
        drawRect(x + 10, y + 17, w - 20, 4, 0xFFD5DCE7);
        drawRect(x + 18, y + 33, w - 36, 18, 0xFF496B48);
        drawRect(x + 26, y + 29, 10, 28, 0xFF2F4A36);
        drawRect(x + w - 36, y + 27, 10, 31, 0xFF2F4A36);
        drawRect(x + 8, y + h - 12, w - 16, 3, 0xFF5FA06F);
    } else if (style == 3) {
        /* Racing / orange scene */
        drawRect(x, y, w, h, 0xFF32251B);
        drawRect(x, y + 8, w, 12, 0xFFA85A24);
        drawRect(x + 13, y + 27, w - 26, 26, 0xFF6E3B26);
        drawRect(x + 23, y + 35, w - 46, 11, 0xFF17191B);
        drawRect(x + 31, y + 32, 9, 17, 0xFFE0CFB2);
        drawRect(x + w - 40, y + 32, 9, 17, 0xFFE0CFB2);
        drawRect(x + 7, y + h - 12, w - 14, 3, 0xFFE0813B);
    } else if (style == 4) {
        /* PSP / blue UI style */
        drawRect(x, y, w, h, 0xFF1D304B);
        drawRect(x + 12, y + 12, w - 24, 8, 0xFF396EAB);
        drawRect(x + 19, y + 30, w - 38, 10, 0xFF87A9D0);
        drawRect(x + 29, y + 49, w - 58, 14, 0xFF2B547C);
        drawRect(x + 10, y + h - 12, w - 20, 3, 0xFF5C92CE);
    } else {
        /* Homebrew / purple scene */
        drawRect(x, y, w, h, 0xFF2B2140);
        drawRect(x + 10, y + 11, w - 20, 9, 0xFF684D91);
        drawRect(x + 18, y + 30, w - 36, 10, 0xFF8D75B8);
        drawRect(x + 28, y + 47, w - 56, 15, 0xFF47336D);
        drawRect(x + 8, y + h - 12, w - 16, 3, 0xFF9174C8);
    }
}

static void drawThumbnail(int x, int y, int w, int h, int selected, int style, const char *duration)
{
    u32 border = selected ? COLOR_RED : COLOR_LINE;

    drawRoundedPanel(x, y, w, h, border);
    drawRoundedPanel(x + 2, y + 2, w - 4, h - 4, COLOR_PANEL2);
    drawThumbnailArt(x + 3, y + 3, w - 6, h - 6, style);

    drawRoundedPanel(x + w - 51, y + h - 22, 44, 17, 0xD8000000);
    drawText(x + w - 46, y + h - 9, duration, 0.43f, COLOR_WHITE);

    drawRoundedPanel(x + 7, y + 7, 24, 24, 0x98000000);
    drawPlayIcon(x + 19, y + 19, 5, COLOR_WHITE);
}

/* ------------------------------------------------------------- */
/* Header / cards                                                 */
/* ------------------------------------------------------------- */

static void drawHeader(const char *pageTitle, int showSearch)
{
    /* Compact, graphical YouTube-style top bar. Text is only metadata. */
    drawRect(0, 0, SCREEN_WIDTH, 47, COLOR_PANEL);
    drawRect(0, 46, SCREEN_WIDTH, 1, COLOR_LINE);

    /* YouTube logo: graphic only, no brand text. */
    drawYoutubeLogo(15, 10, 40, 25);

    /* Current section marker. */
    drawText(66, 28, pageTitle, 0.56f, COLOR_TEXT);
    drawRect(66, 34, 34, 2, COLOR_RED);

    if (showSearch) {
        drawRoundedPanel(173, 8, 235, 31, COLOR_PANEL2);
        drawRoundedOutline(173, 8, 235, 31, COLOR_LINE);
        drawSearchIcon(189, 23, COLOR_MUTED);
        if (searchQuery[0])
            drawTextClipped(202, 29, 190, searchQuery, 0.48f, COLOR_TEXT);
        else
            drawTextClipped(202, 29, 190, "Search", 0.48f, COLOR_MUTED);

        drawRoundedPanel(416, 9, 48, 29, COLOR_BG2);
        drawRoundedOutline(416, 9, 48, 29, COLOR_LINE);
        drawText(432, 28, "R", 0.47f, COLOR_WHITE);
    }
}

static void drawCardSelection(int x, int y, int w, int h, int selected)
{
    if (selected) {
        u32 pulse = ((frameCounter / 8) & 1) ? 0xFF0000FF : 0xD50000FF;
        drawRoundedOutline(x, y, w, h, pulse);
        drawRoundedOutline(x + 1, y + 1, w - 2, h - 2, COLOR_RED);
    } else {
        drawRoundedOutline(x, y, w, h, COLOR_LINE);
    }
}

static void drawVideoCard(int index, int x, int y, int w, int h, int selected)
{
    const Video *v = &videos[index];
    int thumbW = 112;
    int thumbH = h - 12;
    u32 edge = selected ? COLOR_RED : COLOR_LINE;

    /* Physical card surface. */
    drawRoundedPanel(x, y, w, h, COLOR_PANEL);
    drawRoundedOutline(x, y, w, h, edge);

    /* Large graphical thumbnail dominates the card. */
    drawThumbnail(x + 6, y + 6, thumbW, thumbH, selected, v->style, v->duration);

    /* Text is secondary to graphics. */
    drawTextClipped(x + 126, y + 24, w - 134, v->cardTitle1, 0.46f, COLOR_TEXT);
    drawTextClipped(x + 126, y + 41, w - 134, v->cardTitle2, 0.43f, COLOR_TEXT);

    drawRect(x + 126, y + 51, 42, 1, COLOR_LINE);
    drawTextClipped(x + 126, y + 65, w - 134, v->channel, 0.39f, COLOR_MUTED);
    drawTextClipped(x + 126, y + 79, w - 134, v->views, 0.36f, COLOR_DIM);

    /* Three-dot graphical action button. */
    drawRoundedPanel(x + w - 26, y + h - 25, 18, 18, COLOR_BG2);
    drawRect(x + w - 20, y + h - 20, 3, 3, COLOR_MUTED);
    drawRect(x + w - 20, y + h - 15, 3, 3, COLOR_MUTED);
    drawRect(x + w - 20, y + h - 10, 3, 3, COLOR_MUTED);

    if (selected) {
        /* Thin, unmistakable red focus frame. */
        drawRoundedOutline(x - 1, y - 1, w + 2, h + 2, COLOR_RED);
        drawRect(x + 2, y + h - 3, w - 4, 2, COLOR_RED);
    }
}

static void drawShortCard(int index, int x, int y, int w, int h, int selected)
{
    const Video *v = &videos[index];
    drawRoundedPanel(x, y, w, h, COLOR_PANEL);
    drawRoundedOutline(x, y, w, h, selected ? COLOR_RED : COLOR_LINE);
    drawThumbnail(x + 6, y + 6, w - 12, 92, selected, v->style, v->duration);
    drawTextClipped(x + 9, y + 119, w - 18, v->cardTitle1, 0.43f, COLOR_TEXT);
    drawTextClipped(x + 9, y + 136, w - 18, v->channel, 0.38f, COLOR_MUTED);
    drawTextClipped(x + 9, y + 152, w - 18, v->views, 0.36f, COLOR_DIM);
    if (selected)
        drawRoundedOutline(x - 1, y - 1, w + 2, h + 2, COLOR_RED);
}

/* ------------------------------------------------------------- */
/* Pages                                                          */
/* ------------------------------------------------------------- */

static void drawHome(int selected)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("HOME", 1);

    drawVideoCard(0, 14, 78, 225, 88, selected == 0);
    drawVideoCard(1, 241, 78, 225, 88, selected == 1);
    drawVideoCard(2, 14, 174, 225, 88, selected == 2);
    drawVideoCard(3, 241, 174, 225, 88, selected == 3);
}

static void drawTrending(int selected)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("TRENDING", 1);

    drawVideoCard(4, 14, 78, 225, 88, selected == 0);
    drawVideoCard(5, 241, 78, 225, 88, selected == 1);
    drawVideoCard(1, 14, 174, 225, 88, selected == 2);
    drawVideoCard(3, 241, 174, 225, 88, selected == 3);
}

static void drawShorts(int selected)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("SHORTS", 1);

    drawShortCard(2, 10, 86, 146, 174, selected == 0);
    drawShortCard(0, 167, 86, 146, 174, selected == 1);
    drawShortCard(5, 324, 86, 146, 174, selected == 2);
}

static void drawLibrary(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("LIBRARY", 1);

    drawRoundedPanel(72, 108, 336, 92, COLOR_PANEL);
    drawRoundedOutline(72, 108, 336, 92, COLOR_LINE);
    drawLibraryIcon(112, 145, COLOR_RED);
    drawText(145, 143, "YOUR LIBRARY", 0.70f, COLOR_TEXT);
    drawText(145, 166, "Saved videos will appear here.", 0.48f, COLOR_MUTED);
}

static void drawSettings(int selected)
{
    static const char *items[] = {"General", "Video", "Network", "About"};
    int i;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("SETTINGS", 1);

    for (i = 0; i < 4; ++i) {
        int y = 87 + i * 39;
        u32 fill = (i == selected) ? COLOR_RED_SOFT : COLOR_PANEL;
        u32 txt = (i == selected) ? COLOR_WHITE : COLOR_MUTED;

        drawRoundedPanel(28, y, 424, 31, fill);
        drawRoundedOutline(28, y, 424, 31, i == selected ? COLOR_RED : COLOR_LINE);
        drawText(45, y + 21, items[i], 0.52f, txt);
        drawText(420, y + 21, ">", 0.58f, txt);
    }
}

static void drawSearchPage(int selected)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("SEARCH", 1);

    if (searchQuery[0] == '\0') {
        drawRoundedPanel(38, 92, 404, 64, COLOR_PANEL);
        drawRoundedOutline(38, 92, 404, 64, COLOR_LINE);
        drawSearchIcon(64, 124, COLOR_RED);
        drawText(84, 120, "Press R", 0.58f, COLOR_TEXT);
        drawText(84, 140, "to search with PSP OSK", 0.43f, COLOR_MUTED);
        return;
    }

    drawText(16, 69, "RESULTS", 0.40f, COLOR_MUTED);
    drawVideoCard(0, 14, 82, 225, 88, selected == 0);
    drawVideoCard(1, 241, 82, 225, 88, selected == 1);
    drawVideoCard(4, 14, 178, 225, 88, selected == 2);
    drawVideoCard(5, 241, 178, 225, 88, selected == 3);
}

static void drawVideoPage(int selectedVideo, int playing)
{
    const Video *v;
    if (selectedVideo < 0) selectedVideo = 0;
    if (selectedVideo >= VIDEO_COUNT) selectedVideo = VIDEO_COUNT - 1;
    v = &videos[selectedVideo];

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader("WATCH", 0);

    /* Large player surface. */
    drawRoundedPanel(14, 58, 452, 128, COLOR_PANEL);
    drawRoundedOutline(14, 58, 452, 128, COLOR_LINE);
    drawThumbnailArt(18, 62, 444, 120, v->style);
    drawRect(18, 62, 444, 120, 0x58000000);

    drawRoundedPanel(202, 99, 76, 44, playing ? COLOR_GREEN : COLOR_RED);
    if (playing)
        drawPauseIcon(240, 121, COLOR_WHITE);
    else
        drawPlayIcon(240, 121, 9, COLOR_WHITE);

    /* Upper-right playback action, as requested. */
    drawRoundedPanel(350, 194, 102, 30, COLOR_RED);
    drawPlayIcon(368, 209, 5, COLOR_WHITE);
    drawText(382, 215, playing ? "PAUSE" : "PLAY", 0.44f, COLOR_WHITE);

    drawTextClipped(16, 211, 320, v->title, 0.53f, COLOR_TEXT);
    drawText(16, 231, v->channel, 0.42f, COLOR_MUTED);
    drawText(110, 231, v->views, 0.39f, COLOR_DIM);
    drawText(208, 231, v->duration, 0.39f, COLOR_DIM);

    /* Separate title/description box. */
    drawRoundedPanel(14, 242, 452, 25, COLOR_PANEL);
    drawRoundedOutline(14, 242, 452, 25, COLOR_LINE);
    drawTextClipped(23, 258, 430, v->description, 0.37f, COLOR_MUTED);
}

/* ------------------------------------------------------------- */
/* Sidebar                                                        */
/* ------------------------------------------------------------- */

static void drawSidebarIcon(int index, int x, int y, u32 color)
{
    if (index == 0)
        drawHomeIcon(x, y, color);
    else if (index == 1)
        drawTrendingIcon(x, y, color);
    else if (index == 2)
        drawShortsIcon(x, y, color);
    else if (index == 3)
        drawLibraryIcon(x, y, color);
    else
        drawGearIcon(x, y, color);
}

static void drawSidebar(int x, int selected, int open)
{
    int i;
    int y;

    if (!open && x <= -SIDEBAR_WIDTH)
        return;

    /* Dim the current page so the side surface reads as a real overlay. */
    if (x > -SIDEBAR_WIDTH)
        drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x70000000);

    drawRect(x + 4, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, 0x85000000);
    drawRect(x, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, COLOR_PANEL);
    drawRect(x + SIDEBAR_WIDTH - 1, 0, 1, SCREEN_HEIGHT, COLOR_LINE);

    drawYoutubeLogo(x + 14, 12, 34, 24);
    drawText(x + 14, 57, "MENU", 0.48f, COLOR_MUTED);
    drawRect(x + 14, 65, SIDEBAR_WIDTH - 28, 1, COLOR_LINE);

    for (i = 0; i < 5; ++i) {
        y = 77 + i * 32;

        if (i == selected) {
            drawRoundedPanel(x + 8, y, SIDEBAR_WIDTH - 16, 28, COLOR_RED_SOFT);
            drawRoundedOutline(x + 8, y, SIDEBAR_WIDTH - 16, 28, COLOR_RED);
        }

        drawSidebarIcon(i, x + 25, y + 14,
                        i == selected ? COLOR_WHITE : COLOR_MUTED);
        drawText(x + 43, y + 19, sidebarItems[i], 0.48f,
                 i == selected ? COLOR_WHITE : COLOR_MUTED);
    }

    drawRect(x + 14, 254, SIDEBAR_WIDTH - 28, 1, COLOR_LINE);
    drawMenuIcon(x + 25, 266, COLOR_DIM);
    drawText(x + 44, 270, "L CLOSE", 0.38f, COLOR_DIM);
}

/* ------------------------------------------------------------- */
/* OSK search                                                     */
/* ------------------------------------------------------------- */

static void asciiToUtf16(const char *src, unsigned short *dst, int maxChars)
{
    int i = 0;
    if (maxChars <= 0)
        return;

    while (src[i] && i < maxChars - 1) {
        dst[i] = (unsigned short)(unsigned char)src[i];
        ++i;
    }
    dst[i] = 0;
}

static void utf16ToAscii(const unsigned short *src, char *dst, int maxChars)
{
    int i = 0;
    if (maxChars <= 0)
        return;

    while (src[i] && i < maxChars - 1) {
        unsigned short c = src[i++];
        dst[i - 1] = (c < 128) ? (char)c : '?';
    }
    dst[i] = '\0';
}

static int runSearchOSK(void)
{
    SceUtilityOskData data;
    SceUtilityOskParams params;
    unsigned short description[64];
    unsigned short input[OSK_MAX + 1];
    unsigned short output[OSK_MAX + 1];
    int done = 0;
    int rc;

    memset(&data, 0, sizeof(data));
    memset(&params, 0, sizeof(params));
    memset(description, 0, sizeof(description));
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));

    asciiToUtf16("Search YouTube", description, 64);
    asciiToUtf16(searchQuery, input, OSK_MAX + 1);

    data.language = PSP_UTILITY_OSK_LANGUAGE_ENGLISH;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.lines = 1;
    data.unk_24 = 1;
    data.desc = description;
    data.intext = input;
    data.outtextlength = OSK_MAX + 1;
    data.outtextlimit = OSK_MAX;
    data.outtext = output;

    params.base.size = sizeof(params);
    params.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    params.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    rc = sceUtilityOskInitStart(&params);
    if (rc < 0)
        return 0;

    while (!done && running) {
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(COLOR_BG);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        drawSearchPage(0);
        sceGuFinish();
        sceGuSync(0, 0);

        switch (sceUtilityOskGetStatus()) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityOskUpdate(1);
            break;
        case PSP_UTILITY_DIALOG_QUIT:
            sceUtilityOskShutdownStart();
            break;
        case PSP_UTILITY_DIALOG_NONE:
            done = 1;
            break;
        default:
            break;
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    if (data.result != PSP_UTILITY_OSK_RESULT_CANCELLED)
        utf16ToAscii(output, searchQuery, SEARCH_MAX + 1);

    return 1;
}

/* ------------------------------------------------------------- */
/* Input                                                           */
/* ------------------------------------------------------------- */

static int menuToPage(int selected)
{
    if (selected == 0) return PAGE_HOME;
    if (selected == 1) return PAGE_TRENDING;
    if (selected == 2) return PAGE_SHORTS;
    if (selected == 3) return PAGE_LIBRARY;
    return PAGE_SETTINGS;
}

static int selectionCountForPage(int page)
{
    if (page == PAGE_SHORTS)
        return 3;
    if (page == PAGE_SETTINGS)
        return 4;
    return 4;
}

static int videoIndexForPage(int page, int selected)
{
    static const int trending[] = {4, 5, 1, 3};
    static const int shorts[] = {2, 0, 5};
    static const int search[] = {0, 1, 4, 5};

    if (page == PAGE_TRENDING)
        return trending[selected & 3];
    if (page == PAGE_SHORTS)
        return shorts[selected % 3];
    if (page == PAGE_SEARCH)
        return search[selected & 3];
    return selected & 3;
}

static void moveSelectionGrid(int page, int *selected, unsigned int pressed)
{
    int row;
    int col;
    int count = selectionCountForPage(page);

    if (page == PAGE_SETTINGS) {
        if (pressed & PSP_CTRL_UP) {
            (*selected)--;
            if (*selected < 0) *selected = count - 1;
        }
        if (pressed & PSP_CTRL_DOWN) {
            (*selected)++;
            if (*selected >= count) *selected = 0;
        }
        return;
    }

    if (page == PAGE_SHORTS) {
        if (pressed & PSP_CTRL_LEFT) {
            (*selected)--;
            if (*selected < 0) *selected = count - 1;
        }
        if (pressed & PSP_CTRL_RIGHT) {
            (*selected)++;
            if (*selected >= count) *selected = 0;
        }
        return;
    }

    row = (*selected) / 2;
    col = (*selected) % 2;

    if (pressed & PSP_CTRL_LEFT) col = 0;
    if (pressed & PSP_CTRL_RIGHT) col = 1;
    if (pressed & PSP_CTRL_UP) row = 0;
    if (pressed & PSP_CTRL_DOWN) row = 1;

    *selected = row * 2 + col;
    if (*selected < 0) *selected = 0;
    if (*selected >= count) *selected = count - 1;
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;
    int page = PAGE_HOME;
    int selected = 0;
    int sidebarSelected = 0;
    int sidebarOpen = 0;
    int sidebarX = -SIDEBAR_WIDTH;
    int sidebarTarget = -SIDEBAR_WIDTH;
    int currentVideoIndex = 0;
    int playing = 0;

    SetupCallbacks();
    initGraphics();
    initFont();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (running) {
        unsigned int pressed;

        ++frameCounter;
        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~oldButtons;
        oldButtons = pad.Buttons;

        if (pressed & PSP_CTRL_LTRIGGER) {
            sidebarOpen = !sidebarOpen;
            sidebarTarget = sidebarOpen ? 0 : -SIDEBAR_WIDTH;
        }

        if (sidebarOpen) {
            if (pressed & PSP_CTRL_UP) {
                --sidebarSelected;
                if (sidebarSelected < 0) sidebarSelected = 4;
            }
            if (pressed & PSP_CTRL_DOWN) {
                ++sidebarSelected;
                if (sidebarSelected > 4) sidebarSelected = 0;
            }
            if (pressed & PSP_CTRL_CROSS) {
                page = menuToPage(sidebarSelected);
                selected = 0;
                playing = 0;
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }
        } else {
            if (pressed & PSP_CTRL_RTRIGGER) {
                page = PAGE_SEARCH;
                runSearchOSK();
                selected = 0;
                oldButtons = 0;
                sceCtrlReadBufferPositive(&pad, 1);
                oldButtons = pad.Buttons;
            } else if (page == PAGE_VIDEO) {
                if (pressed & PSP_CTRL_CROSS)
                    playing = !playing;
                if (pressed & PSP_CTRL_CIRCLE) {
                    page = PAGE_HOME;
                    selected = 0;
                    playing = 0;
                }
            } else {
                moveSelectionGrid(page, &selected, pressed);

                if (pressed & PSP_CTRL_CROSS) {
                    if (page == PAGE_HOME || page == PAGE_TRENDING ||
                        page == PAGE_SHORTS || page == PAGE_SEARCH) {
                        currentVideoIndex = videoIndexForPage(page, selected);
                        page = PAGE_VIDEO;
                        playing = 0;
                    }
                }

                if (pressed & PSP_CTRL_CIRCLE)
                    page = PAGE_HOME;
            }
        }

        if (sidebarX < sidebarTarget) {
            sidebarX += 20;
            if (sidebarX > sidebarTarget) sidebarX = sidebarTarget;
        } else if (sidebarX > sidebarTarget) {
            sidebarX -= 20;
            if (sidebarX < sidebarTarget) sidebarX = sidebarTarget;
        }

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(COLOR_BG);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (page == PAGE_HOME)
            drawHome(selected);
        else if (page == PAGE_TRENDING)
            drawTrending(selected);
        else if (page == PAGE_SHORTS)
            drawShorts(selected);
        else if (page == PAGE_LIBRARY)
            drawLibrary();
        else if (page == PAGE_SETTINGS)
            drawSettings(selected);
        else if (page == PAGE_SEARCH)
            drawSearchPage(selected);
        else
            drawVideoPage(currentVideoIndex, playing);

        drawSidebar(sidebarX, sidebarSelected, sidebarOpen || sidebarX > -SIDEBAR_WIDTH);

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
        sceKernelDelayThread(1000);
    }

    shutdownFont();
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
