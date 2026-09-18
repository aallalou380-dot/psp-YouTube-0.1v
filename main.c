#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <intraFont.h>

PSP_MODULE_INFO("PSP YouTube 0.2v", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUF_WIDTH     512
#define SIDEBAR_WIDTH 126

#define COLOR_BG      0xFF0B0E12
#define COLOR_BG2     0xFF10151B
#define COLOR_SURFACE 0xFF171C22
#define COLOR_SURFACE2 0xFF20262E
#define COLOR_LINE    0xFF343B45
#define COLOR_TEXT    0xFFF4F5F7
#define COLOR_MUTED   0xFF929AA5
#define COLOR_RED     0xFF0000FF
#define COLOR_RED_SOFT 0xC80000FF
#define COLOR_WHITE   0xFFFFFFFF
#define COLOR_BLACK   0xFF000000

static unsigned int __attribute__((aligned(16))) list[262144];
static volatile int running = 1;
static intraFont *uiFont = NULL;
static int fontReady = 0;

typedef struct {
    u32 color;
    short x, y, z;
} Vertex;

typedef struct {
    const char *title;
    const char *channel;
    const char *views;
    const char *duration;
} Video;

static const Video videos[] = {
    {"GTA 5 - Best Moments", "ZetaGamez", "2.4M views", "12:34"},
    {"Marvel's Spider-Man 2", "PlayStation", "5.1M views", "15:42"},
    {"Beautiful Places in 4K", "Nature Relaxation", "1.8M views", "10:28"},
    {"Forza Horizon 5 - Drive", "Xbox", "3.7M views", "18:16"},
    {"PSP Homebrew News", "PSP Dev", "640K views", "04:46"},
    {"New PSP Projects", "Homebrew", "920K views", "08:16"}
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

static int exitCallback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callbackThread(SceSize args, void *argp)
{
    int cbid;
    (void)args; (void)argp;
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

static void drawRect(int x, int y, int w, int h, u32 color)
{
    Vertex *v;
    if (w <= 0 || h <= 0)
        return;

    sceGuDisable(GU_TEXTURE_2D);
    v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].color = color;
    v[0].x = (short)x;     v[0].y = (short)y;     v[0].z = 0;
    v[1].color = color;
    v[1].x = (short)(x+w); v[1].y = (short)(y+h); v[1].z = 0;
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
    int r = 6;
    drawRect(x + r, y, w - 2*r, h, color);
    drawRect(x, y + r, r, h - 2*r, color);
    drawRect(x + w - r, y + r, r, h - 2*r, color);
    drawRect(x + r/2, y + r/2, w - r, h - r, color);
    drawRect(x + 2, y + 2, w - 4, 2, color);
}

static void drawText(float x, float y, const char *text, float size, u32 color)
{
    if (!fontReady || !uiFont || !text)
        return;
    intraFontSetStyle(uiFont, size, color, 0, 0.0f, 0);
    intraFontPrint(uiFont, x, y, text);
}

static void drawTextShadow(float x, float y, const char *text, float size, u32 color)
{
    if (!fontReady || !uiFont || !text)
        return;
    intraFontSetStyle(uiFont, size, color, 0x70000000, 0.0f, 0);
    intraFontPrint(uiFont, x, y, text);
}

static void drawPlayIcon(int cx, int cy, int size, u32 color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(3 * sizeof(Vertex));
    v[0].color = color; v[0].x = (short)(cx - size/2); v[0].y = (short)(cy - size);   v[0].z = 0;
    v[1].color = color; v[1].x = (short)(cx - size/2); v[1].y = (short)(cy + size);   v[1].z = 0;
    v[2].color = color; v[2].x = (short)(cx + size);   v[2].y = (short)cy;             v[2].z = 0;
    sceGuDrawArray(GU_TRIANGLES,
                   GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   3, NULL, v);
}

static void drawMenuIcon(int cx, int cy, u32 color)
{
    drawRect(cx - 8, cy - 7, 16, 2, color);
    drawRect(cx - 8, cy - 1, 16, 2, color);
    drawRect(cx - 8, cy + 5, 16, 2, color);
}

static void drawSearchIcon(int cx, int cy, u32 color)
{
    int i;
    for (i = 0; i < 14; ++i) {
        double a = (double)i / 14.0 * 6.2831853;
        int px = cx - 3 + (int)(5.0 * cos(a));
        int py = cy - 2 + (int)(5.0 * sin(a));
        drawRect(px, py, 2, 2, color);
    }
    drawLine(cx + 2, cy + 3, cx + 9, cy + 10, 2, color);
}

static void drawHomeIcon(int x, int y, u32 color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(3 * sizeof(Vertex));
    v[0].color = color; v[0].x = (short)x;     v[0].y = (short)(y-7); v[0].z = 0;
    v[1].color = color; v[1].x = (short)(x-9); v[1].y = (short)(y+1); v[1].z = 0;
    v[2].color = color; v[2].x = (short)(x+9); v[2].y = (short)(y+1); v[2].z = 0;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 3, NULL, v);
    drawRect(x-6, y, 12, 9, color);
}

static void drawFlameIcon(int x, int y, u32 color)
{
    drawRect(x-4, y-7, 8, 14, color);
    drawRect(x-7, y-2, 14, 8, color);
    drawRect(x-3, y+4, 6, 6, color);
}

static void drawGearIcon(int x, int y, u32 color)
{
    drawRect(x-7, y-2, 14, 5, color);
    drawRect(x-2, y-7, 5, 14, color);
    drawRect(x-5, y-5, 4, 4, color);
    drawRect(x+2, y+2, 4, 4, color);
    drawRect(x-2, y-2, 4, 4, COLOR_SURFACE);
}

static void drawLibraryIcon(int x, int y, u32 color)
{
    drawRect(x-9, y-6, 18, 4, color);
    drawRect(x-7, y, 14, 4, color);
    drawRect(x-5, y+6, 10, 4, color);
}

static void drawShortsIcon(int x, int y, u32 color)
{
    drawRect(x-6, y-8, 12, 4, color);
    drawRect(x-6, y+4, 12, 4, color);
    drawRect(x-8, y-5, 4, 10, color);
    drawRect(x+4, y-5, 4, 10, color);
}

static void drawHeader(int page)
{
    const char *title = "HOME";

    if (page == PAGE_TRENDING) title = "TRENDING";
    else if (page == PAGE_SHORTS) title = "SHORTS";
    else if (page == PAGE_SEARCH) title = "SEARCH";
    else if (page == PAGE_SETTINGS) title = "SETTINGS";
    else if (page == PAGE_LIBRARY) title = "LIBRARY";
    else if (page == PAGE_VIDEO) title = "VIDEO";

    drawRect(0, 0, SCREEN_WIDTH, 43, COLOR_SURFACE);
    drawRect(0, 42, SCREEN_WIDTH, 1, COLOR_LINE);

    drawRoundedPanel(14, 9, 30, 24, COLOR_RED);
    drawPlayIcon(29, 21, 7, COLOR_WHITE);
    drawTextShadow(51, 27, "PSP YouTube", 0.67f, COLOR_TEXT);
    drawText(150, 27, "0.2v", 0.46f, COLOR_MUTED);

    drawRoundedPanel(278, 9, 128, 24, COLOR_BG2);
    drawSearchIcon(293, 20, COLOR_MUTED);
    drawText(304, 26, "SEARCH", 0.50f, COLOR_MUTED);

    drawRoundedPanel(417, 10, 28, 22, COLOR_BG2);
    drawText(425, 26, "R", 0.52f, COLOR_TEXT);

    drawText(24, 64, title, 0.82f, COLOR_TEXT);
    drawRect(24, 71, 50, 2, COLOR_RED_SOFT);
}

static void drawThumbnail(int x, int y, int w, int h, int selected, int style, const char *duration)
{
    u32 edge = selected ? COLOR_RED_SOFT : COLOR_LINE;
    u32 base = COLOR_SURFACE2;

    drawRoundedPanel(x, y, w, h, edge);
    drawRoundedPanel(x+2, y+2, w-4, h-4, base);

    if (style == 0) {
        drawRect(x+3, y+3, w-6, h/2, 0xFF443026);
        drawRect(x+3, y+h/2+2, w-6, h/2-5, 0xFF152B37);
        drawRect(x+22, y+15, w-44, 7, 0xFF7D4B35);
        drawRect(x+12, y+h-28, w-24, 8, 0xFF2C4651);
    } else if (style == 1) {
        drawRect(x+3, y+3, w-6, h-6, 0xFF35242A);
        drawRect(x+16, y+13, w/2, 10, 0xFF77313B);
        drawRect(x+w/2-5, y+30, w/2-12, 28, 0xFF5B2835);
        drawRect(x+10, y+h-19, w-20, 6, 0xFF7D434E);
    } else if (style == 2) {
        drawRect(x+3, y+3, w-6, h-6, 0xFF213A32);
        drawRect(x+10, y+12, w-20, 22, 0xFF4E6C50);
        drawRect(x+24, y+38, w-48, 12, 0xFF6C8560);
        drawRect(x+7, y+h-18, w-14, 5, 0xFF8CA57B);
    } else {
        drawRect(x+3, y+3, w-6, h-6, 0xFF352C1F);
        drawRect(x+11, y+14, w-22, 9, 0xFF9A6631);
        drawRect(x+19, y+31, w-38, 20, 0xFF5C442B);
        drawRect(x+6, y+h-18, w-12, 5, 0xFFB17B3B);
    }

    drawRoundedPanel(x+w-54, y+h-23, 47, 18, 0xD0000000);
    drawText(x+w-48, y+h-10, duration, 0.47f, COLOR_WHITE);

    drawPlayIcon(x+w/2, y+h/2, 8, 0xE5FFFFFF);

    if (selected) {
        drawRect(x+3, y+h-5, w-6, 2, COLOR_RED);
    }
}

static void drawVideoCard(int index, int x, int y, int w, int h, int selected)
{
    drawRoundedPanel(x, y, w, h, selected ? COLOR_RED_SOFT : COLOR_SURFACE);
    drawThumbnail(x+6, y+6, w-12, 78, selected, index % 4, videos[index].duration);

    drawText(x+9, y+100, videos[index].title, 0.57f, COLOR_TEXT);
    drawText(x+9, y+117, videos[index].channel, 0.44f, COLOR_MUTED);
    drawText(x+9, y+133, videos[index].views, 0.42f, COLOR_MUTED);

    drawRoundedPanel(x+w-29, y+98, 20, 20, COLOR_BG2);
    drawRect(x+w-20, y+103, 3, 3, COLOR_MUTED);
    drawRect(x+w-20, y+109, 3, 3, COLOR_MUTED);
    drawRect(x+w-20, y+115, 3, 3, COLOR_MUTED);
}

static void drawHome(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_HOME);
    drawText(24, 91, "RECOMMENDED", 0.56f, COLOR_MUTED);

    drawVideoCard(0, 18, 103, 214, 157, selectedVideo == 0);
    drawVideoCard(1, 248, 103, 214, 157, selectedVideo == 1);
}

static void drawTrending(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_TRENDING);
    drawText(24, 91, "POPULAR NOW", 0.56f, COLOR_MUTED);

    drawVideoCard(4, 18, 103, 214, 157, selectedVideo == 4);
    drawVideoCard(5, 248, 103, 214, 157, selectedVideo == 5);
}

static void drawShorts(int selectedVideo)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SHORTS);
    drawText(24, 91, "SHORT VIDEOS", 0.56f, COLOR_MUTED);

    drawVideoCard(2, 18, 103, 138, 157, selectedVideo == 2);
    drawVideoCard(3, 171, 103, 138, 157, selectedVideo == 3);
    drawVideoCard(4, 324, 103, 138, 157, selectedVideo == 4);
}

static void drawSearchPage(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SEARCH);

    drawRoundedPanel(28, 104, 330, 38, COLOR_SURFACE2);
    drawSearchIcon(47, 122, COLOR_MUTED);
    drawText(62, 128, "Search YouTube", 0.58f, COLOR_MUTED);

    drawRoundedPanel(367, 104, 85, 38, COLOR_RED_SOFT);
    drawText(388, 128, "SEARCH", 0.54f, COLOR_WHITE);

    drawText(29, 177, "Press X to enter a search", 0.54f, COLOR_MUTED);
    drawText(29, 199, "Results will appear here.", 0.46f, COLOR_MUTED);
}

static void drawSettings(int selected)
{
    const char *items[] = { "GENERAL", "VIDEO", "NETWORK", "ABOUT" };
    int i;

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_SETTINGS);

    for (i = 0; i < 4; ++i) {
        u32 panel = (i == selected) ? COLOR_RED_SOFT : COLOR_SURFACE;
        drawRoundedPanel(24, 95 + i*36, 432, 30, panel);
        drawText(39, 116 + i*36, items[i], 0.55f,
                 (i == selected) ? COLOR_WHITE : COLOR_MUTED);
        drawText(424, 116 + i*36, ">", 0.56f,
                 (i == selected) ? COLOR_WHITE : COLOR_MUTED);
    }
}

static void drawLibrary(void)
{
    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_LIBRARY);
    drawRoundedPanel(38, 106, 404, 86, COLOR_SURFACE);
    drawLibraryIcon(76, 143, COLOR_RED_SOFT);
    drawText(101, 140, "YOUR LIBRARY", 0.73f, COLOR_TEXT);
    drawText(101, 162, "Saved videos will appear here.", 0.48f, COLOR_MUTED);
}

static void drawVideoPage(int selectedVideo)
{
    const Video *v;
    if (selectedVideo < 0) selectedVideo = 0;
    if (selectedVideo >= VIDEO_COUNT) selectedVideo = VIDEO_COUNT - 1;
    v = &videos[selectedVideo];

    drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    drawHeader(PAGE_VIDEO);

    drawThumbnail(20, 93, 270, 128, 0, selectedVideo % 4, v->duration);
    drawText(307, 112, v->title, 0.63f, COLOR_TEXT);
    drawText(307, 137, v->channel, 0.49f, COLOR_MUTED);
    drawText(307, 158, v->views, 0.45f, COLOR_MUTED);

    drawRoundedPanel(307, 179, 115, 34, COLOR_RED_SOFT);
    drawPlayIcon(328, 196, 6, COLOR_WHITE);
    drawText(342, 202, "PLAY", 0.53f, COLOR_WHITE);
}

static void drawSidebar(int x, int selected)
{
    const char *items[] = { "HOME", "TRENDING", "SHORTS", "SEARCH", "SETTINGS", "LIBRARY" };
    int i;
    int y;

    if (x <= -SIDEBAR_WIDTH)
        return;

    drawRect(x, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT, 0xF5171C22);
    drawRect(x + SIDEBAR_WIDTH - 1, 0, 1, SCREEN_HEIGHT, COLOR_LINE);

    drawRoundedPanel(x+12, 11, 30, 24, COLOR_RED);
    drawPlayIcon(x+27, 23, 7, COLOR_WHITE);
    drawText(x+50, 28, "MENU", 0.55f, COLOR_TEXT);
    drawText(x+14, 54, "PSP YouTube", 0.47f, COLOR_MUTED);
    drawRect(x+14, 61, SIDEBAR_WIDTH-28, 1, COLOR_LINE);

    for (i = 0; i < 6; ++i) {
        y = 72 + i * 30;
        if (i == selected) {
            drawRoundedPanel(x+7, y, SIDEBAR_WIDTH-14, 26, COLOR_RED_SOFT);
        }

        switch (i) {
            case 0: drawHomeIcon(x+23, y+13, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
            case 1: drawFlameIcon(x+23, y+13, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
            case 2: drawShortsIcon(x+23, y+13, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
            case 3: drawSearchIcon(x+21, y+11, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
            case 4: drawGearIcon(x+23, y+13, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
            case 5: drawLibraryIcon(x+23, y+13, i == selected ? COLOR_WHITE : COLOR_MUTED); break;
        }

        drawText(x+39, y+18, items[i], 0.46f,
                 i == selected ? COLOR_WHITE : COLOR_MUTED);
    }

    drawRect(x+14, 258, SIDEBAR_WIDTH-28, 1, COLOR_LINE);
    drawText(x+15, 267, "L CLOSE", 0.40f, COLOR_MUTED);
}

static int menuToPage(int selected)
{
    if (selected == 0) return PAGE_HOME;
    if (selected == 1) return PAGE_TRENDING;
    if (selected == 2) return PAGE_SHORTS;
    if (selected == 3) return PAGE_SEARCH;
    if (selected == 4) return PAGE_SETTINGS;
    return PAGE_LIBRARY;
}

static void updateSelection(int page, unsigned int pressed, int *selectedVideo, int *selectedSetting)
{
    if (page == PAGE_HOME) {
        if (pressed & PSP_CTRL_LEFT)  *selectedVideo = (*selectedVideo == 1) ? 0 : *selectedVideo;
        if (pressed & PSP_CTRL_RIGHT) *selectedVideo = (*selectedVideo == 0) ? 1 : *selectedVideo;
        if (pressed & PSP_CTRL_CROSS) {}
    } else if (page == PAGE_TRENDING) {
        if (pressed & PSP_CTRL_LEFT)  *selectedVideo = 4;
        if (pressed & PSP_CTRL_RIGHT) *selectedVideo = 5;
    } else if (page == PAGE_SHORTS) {
        if (pressed & PSP_CTRL_LEFT) {
            if (*selectedVideo == 3) *selectedVideo = 2;
            else if (*selectedVideo == 4) *selectedVideo = 3;
        }
        if (pressed & PSP_CTRL_RIGHT) {
            if (*selectedVideo == 2) *selectedVideo = 3;
            else if (*selectedVideo == 3) *selectedVideo = 4;
        }
    } else if (page == PAGE_SETTINGS) {
        if (pressed & PSP_CTRL_UP) {
            (*selectedSetting)--;
            if (*selectedSetting < 0) *selectedSetting = 3;
        }
        if (pressed & PSP_CTRL_DOWN) {
            (*selectedSetting)++;
            if (*selectedSetting > 3) *selectedSetting = 0;
        }
    }
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;
    int page = PAGE_HOME;
    int selectedVideo = 0;
    int selectedSetting = 0;
    int sidebarSelected = 0;
    int sidebarX = -SIDEBAR_WIDTH;
    int sidebarTarget = -SIDEBAR_WIDTH;
    int sidebarOpen = 0;

    SetupCallbacks();
    initGraphics();
    initFont();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (running) {
        unsigned int pressed;

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~oldButtons;
        oldButtons = pad.Buttons;

        if (pressed & PSP_CTRL_LTRIGGER) {
            sidebarOpen = !sidebarOpen;
            sidebarTarget = sidebarOpen ? 0 : -SIDEBAR_WIDTH;
        }

        if (sidebarOpen) {
            if (pressed & PSP_CTRL_UP) {
                sidebarSelected--;
                if (sidebarSelected < 0) sidebarSelected = 5;
            }
            if (pressed & PSP_CTRL_DOWN) {
                sidebarSelected++;
                if (sidebarSelected > 5) sidebarSelected = 0;
            }
            if (pressed & PSP_CTRL_CROSS) {
                page = menuToPage(sidebarSelected);
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                sidebarOpen = 0;
                sidebarTarget = -SIDEBAR_WIDTH;
            }
        } else {
            updateSelection(page, pressed, &selectedVideo, &selectedSetting);

            if (pressed & PSP_CTRL_CIRCLE) {
                if (page == PAGE_VIDEO)
                    page = PAGE_HOME;
                else
                    page = PAGE_HOME;
            }

            if (pressed & PSP_CTRL_CROSS) {
                if (page == PAGE_HOME || page == PAGE_TRENDING || page == PAGE_SHORTS)
                    page = PAGE_VIDEO;
            }

            if (page == PAGE_VIDEO && (pressed & PSP_CTRL_START))
                page = PAGE_HOME;
        }

        if (sidebarX < sidebarTarget) {
            sidebarX += 18;
            if (sidebarX > sidebarTarget) sidebarX = sidebarTarget;
        } else if (sidebarX > sidebarTarget) {
            sidebarX -= 18;
            if (sidebarX < sidebarTarget) sidebarX = sidebarTarget;
        }

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(COLOR_BG);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (page == PAGE_HOME) drawHome(selectedVideo);
        else if (page == PAGE_TRENDING) drawTrending(selectedVideo);
        else if (page == PAGE_SHORTS) drawShorts(selectedVideo);
        else if (page == PAGE_SEARCH) drawSearchPage();
        else if (page == PAGE_SETTINGS) drawSettings(selectedSetting);
        else if (page == PAGE_LIBRARY) drawLibrary();
        else if (page == PAGE_VIDEO) drawVideoPage(selectedVideo);

        drawSidebar(sidebarX, sidebarSelected);

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
