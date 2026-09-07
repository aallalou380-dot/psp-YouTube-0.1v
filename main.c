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

static unsigned int __attribute__((aligned(16))) list[262144];

/* --------------------------------------------------------- */
/* Colors                                                     */
/* --------------------------------------------------------- */

#define RGB(r,g,b) (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))

#define COLOR_BG       RGB(16,16,16)
#define COLOR_PANEL    RGB(23,23,23)
#define COLOR_CARD     RGB(35,35,35)
#define COLOR_CARD2    RGB(43,43,43)
#define COLOR_TEXT     RGB(245,245,245)
#define COLOR_DIM      RGB(150,150,150)
#define COLOR_RED      RGB(230,35,35)
#define COLOR_SELECT   RGB(52,52,52)
#define COLOR_BORDER   RGB(90,90,90)
#define COLOR_BLACK    RGB(8,8,8)

/* --------------------------------------------------------- */
/* PSP callbacks                                               */
/* --------------------------------------------------------- */

int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback(
        "Exit Callback",
        exit_callback,
        NULL
    );

    if (cbid >= 0)
        sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();

    return 0;
}

int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread(
        "CallbackThread",
        CallbackThread,
        0x11,
        0xFA0,
        0,
        NULL
    );

    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);

    return thid;
}

/* --------------------------------------------------------- */
/* Graphics                                                    */
/* --------------------------------------------------------- */

void initGraphics(void)
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
        2048 - SCREEN_WIDTH / 2,
        2048 - SCREEN_HEIGHT / 2
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuDepthRange(
        65535,
        0
    );

    sceGuScissor(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHTING);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* --------------------------------------------------------- */
/* Basic drawing                                               */
/* --------------------------------------------------------- */

typedef struct {
    float x;
    float y;
    float z;
} Vertex;

void drawRect(
    int x,
    int y,
    int width,
    int height,
    unsigned int color
)
{
    Vertex *v;

    if (width <= 0 || height <= 0)
        return;

    sceGuColor(color);

    v = (Vertex *)sceGuGetMemory(
        sizeof(Vertex) * 2
    );

    v[0].x = (float)x;
    v[0].y = (float)y;
    v[0].z = 0.0f;

    v[1].x = (float)(x + width);
    v[1].y = (float)(y + height);
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2,
        NULL,
        v
    );
}

void drawBorder(
    int x,
    int y,
    int width,
    int height,
    int thickness,
    unsigned int color
)
{
    if (width <= 0 || height <= 0 || thickness <= 0)
        return;

    drawRect(
        x,
        y,
        width,
        thickness,
        color
    );

    drawRect(
        x,
        y + height - thickness,
        width,
        thickness,
        color
    );

    drawRect(
        x,
        y,
        thickness,
        height,
        color
    );

    drawRect(
        x + width - thickness,
        y,
        thickness,
        height,
        color
    );
}

/* --------------------------------------------------------- */
/* Font                                                        */
/* --------------------------------------------------------- */

static const unsigned char font5x5[37][5] = {
    {0,0,0,0,0},

    {14,17,31,17,17},
    {30,17,30,17,30},
    {14,17,16,17,14},
    {30,17,17,17,30},
    {31,16,30,16,31},
    {31,16,30,16,16},
    {14,16,23,17,14},
    {17,17,31,17,17},
    {14,4,4,4,14},
    {7,2,2,18,12},
    {17,18,28,18,17},
    {16,16,16,16,31},
    {17,27,21,17,17},
    {17,25,21,19,17},
    {14,17,17,17,14},
    {30,17,30,16,16},
    {14,17,17,21,14},
    {30,17,30,18,17},
    {15,16,14,1,30},
    {31,4,4,4,4},
    {17,17,17,17,14},
    {17,17,17,10,4},
    {17,17,21,27,17},
    {17,10,4,10,17},
    {17,10,4,4,4},
    {31,2,4,8,31},

    {14,17,19,21,14},
    {4,12,4,4,14},
    {14,17,2,4,31},
    {30,1,6,1,30},
    {2,6,10,31,2},
    {31,16,30,1,30},
    {6,8,30,17,14},
    {31,1,2,4,8},
    {14,17,14,17,14},
    {14,17,15,1,14}
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

void drawChar(
    int x,
    int y,
    char c,
    int scale,
    unsigned int color
)
{
    int row;
    int col;
    int idx = fontIndex(c);

    for (row = 0; row < 5; row++)
    {
        unsigned char bits = font5x5[idx][row];

        for (col = 0; col < 5; col++)
        {
            if (bits & (1 << (4 - col)))
            {
                drawRect(
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

void drawText(
    int x,
    int y,
    const char *text,
    unsigned int color
)
{
    int i = 0;

    while (text[i] != '\0')
    {
        drawChar(
            x + i * 6,
            y,
            text[i],
            1,
            color
        );

        i++;
    }
}

void drawTextLarge(
    int x,
    int y,
    const char *text,
    unsigned int color
)
{
    int i = 0;

    while (text[i] != '\0')
    {
        drawChar(
            x + i * 12,
            y,
            text[i],
            2,
            color
        );

        i++;
    }
}

/* --------------------------------------------------------- */
/* Video data                                                  */
/* --------------------------------------------------------- */

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

/* --------------------------------------------------------- */
/* Application states                                          */
/* --------------------------------------------------------- */

#define PAGE_HOME       0
#define PAGE_TRENDING   1
#define PAGE_SEARCH     2
#define PAGE_LIBRARY    3
#define PAGE_SETTINGS   4
#define PAGE_VIDEO      5

/* --------------------------------------------------------- */
/* Thumbnail                                                   */
/* --------------------------------------------------------- */

void drawThumbnail(
    int x,
    int y,
    int width,
    int height,
    int selected
)
{
    drawRect(
        x,
        y,
        width,
        height,
        COLOR_CARD
    );

    drawRect(
        x + 1,
        y + 1,
        width - 2,
        height - 2,
        COLOR_CARD2
    );

    /*
     * Temporary video symbol.
     * Real thumbnails will be added later.
     */

    drawRect(
        x + 10,
        y + 10,
        width - 20,
        3,
        COLOR_DIM
    );

    drawRect(
        x + 10,
        y + height - 13,
        width - 20,
        3,
        COLOR_DIM
    );

    /*
     * Play symbol
     */

    drawRect(
        x + width / 2 - 7,
        y + height / 2 - 14,
        7,
        28,
        COLOR_RED
    );

    drawRect(
        x + width / 2,
        y + height / 2 - 10,
        7,
        20,
        COLOR_RED
    );

    drawRect(
        x + width / 2 + 7,
        y + height / 2 - 5,
        7,
        10,
        COLOR_RED
    );

    if (selected)
    {
        drawBorder(
            x - 2,
            y - 2,
            width + 4,
            height + 4,
            2,
            COLOR_RED
        );
    }
}

/* --------------------------------------------------------- */
/* Header                                                       */
/* --------------------------------------------------------- */

void drawHeader(int page)
{
    const char *titles[] = {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS",
        "VIDEO"
    };

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        45,
        COLOR_PANEL
    );

    drawTextLarge(
        18,
        13,
        titles[page],
        COLOR_TEXT
    );

    /*
     * Search indicator
     */

    drawBorder(
        350,
        10,
        82,
        22,
        1,
        COLOR_BORDER
    );

    drawText(
        360,
        17,
        "SEARCH",
        COLOR_DIM
    );

    /*
     * Magnifier
     */

    drawBorder(
        443,
        10,
        12,
        12,
        2,
        COLOR_TEXT
    );

    drawRect(
        453,
        21,
        6,
        2,
        COLOR_TEXT
    );
}

/* --------------------------------------------------------- */
/* Sidebar                                                     */
/* --------------------------------------------------------- */

#define SIDEBAR_WIDTH 125

void drawSidebar(
    int selectedMenu,
    int sidebarX
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
    int y = 75;

    /*
     * Shadow
     */

    if (sidebarX >= 0)
    {
        drawRect(
            sidebarX + SIDEBAR_WIDTH,
            0,
            8,
            SCREEN_HEIGHT,
            COLOR_BLACK
        );
    }

    /*
     * Main panel
     */

    drawRect(
        sidebarX,
        0,
        SIDEBAR_WIDTH,
        SCREEN_HEIGHT,
        COLOR_PANEL
    );

    /*
     * Border
     */

    drawRect(
        sidebarX + SIDEBAR_WIDTH - 1,
        0,
        1,
        SCREEN_HEIGHT,
        COLOR_BORDER
    );

    /*
     * Logo
     */

    drawRect(
        sidebarX + 15,
        13,
        31,
        21,
        COLOR_RED
    );

    drawRect(
        sidebarX + 28,
        18,
        8,
        11,
        COLOR_TEXT
    );

    drawTextLarge(
        sidebarX + 52,
        15,
        "PSP",
        COLOR_TEXT
    );

    drawText(
        sidebarX + 15,
        42,
        "YOUTUBE",
        COLOR_DIM
    );

    /*
     * Menu
     */

    for (i = 0; i < 5; i++)
    {
        if (i == selectedMenu)
        {
            drawRect(
                sidebarX + 8,
                y - 8,
                108,
                25,
                COLOR_SELECT
            );

            drawRect(
                sidebarX + 8,
                y - 8,
                3,
                25,
                COLOR_RED
            );
        }

        drawText(
            sidebarX + 20,
            y,
            items[i],
            i == selectedMenu
                ? COLOR_TEXT
                : COLOR_DIM
        );

        y += 32;
    }

    /*
     * Bottom hint
     */

    drawText(
        sidebarX + 17,
        245,
        "L CLOSE",
        COLOR_DIM
    );
}

/* --------------------------------------------------------- */
/* Home                                                        */
/* --------------------------------------------------------- */

void drawHome(int selectedVideo)
{
    const int cardWidth = 150;
    const int cardHeight = 70;

    const int x1 = 20;
    const int x2 = 190;

    /*
     * Background
     */

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_HOME);

    /*
     * Section title
     */

    drawTextLarge(
        18,
        53,
        "RECOMMENDED",
        COLOR_TEXT
    );

    /*
     * First row
     */

    drawThumbnail(
        x1,
        70,
        cardWidth,
        cardHeight,
        selectedVideo == 0
    );

    drawThumbnail(
        x2,
        70,
        cardWidth,
        cardHeight,
        selectedVideo == 1
    );

    drawText(
        x1,
        147,
        videos[0].title,
        COLOR_TEXT
    );

    drawText(
        x1,
        158,
        videos[0].channel,
        COLOR_DIM
    );

    drawText(
        x2,
        147,
        videos[1].title,
        COLOR_TEXT
    );

    drawText(
        x2,
        158,
        videos[1].channel,
        COLOR_DIM
    );

    /*
     * Second row
     */

    drawThumbnail(
        x1,
        183,
        cardWidth,
        cardHeight,
        selectedVideo == 2
    );

    drawThumbnail(
        x2,
        183,
        cardWidth,
        cardHeight,
        selectedVideo == 3
    );

    drawText(
        x1,
        260,
        videos[2].title,
        COLOR_TEXT
    );

    drawText(
        x2,
        260,
        videos[3].title,
        COLOR_TEXT
    );
}

/* --------------------------------------------------------- */
/* Trending                                                     */
/* --------------------------------------------------------- */

void drawTrending(int selectedVideo)
{
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_TRENDING);

    drawTextLarge(
        18,
        53,
        "TRENDING",
        COLOR_TEXT
    );

    drawThumbnail(
        20,
        70,
        150,
        70,
        selectedVideo == 0
    );

    drawText(
        20,
        147,
        videos[1].title,
        COLOR_TEXT
    );

    drawText(
        20,
        158,
        videos[1].channel,
        COLOR_DIM
    );

    drawThumbnail(
        190,
        70,
        150,
        70,
        selectedVideo == 1
    );

    drawText(
        190,
        147,
        videos[3].title,
        COLOR_TEXT
    );

    drawText(
        190,
        158,
        videos[3].channel,
        COLOR_DIM
    );

    drawThumbnail(
        20,
        183,
        150,
        70,
        selectedVideo == 2
    );

    drawText(
        20,
        260,
        videos[4].title,
        COLOR_TEXT
    );

    drawThumbnail(
        190,
        183,
        150,
        70,
        selectedVideo == 3
    );

    drawText(
        190,
        260,
        videos[5].title,
        COLOR_TEXT
    );
}

/* --------------------------------------------------------- */
/* Search                                                       */
/* --------------------------------------------------------- */

void drawSearch(void)
{
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawHeader(PAGE_SEARCH);

    drawTextLarge(
        20,
        58,
        "SEARCH",
        COLOR_TEXT
    );

    /*
     * Search field
     */

    drawBorder(
        20,
        90,
        330,
        35,
        2,
        COLOR_BORDER
    );

    drawText(
        35,
        105,
        "TYPE SEARCH HERE",
        COLOR_DIM
    );

    /*
     * Search button
     */

    drawRect(
        365,
        90,
        90,
        35,
        COLOR_RED
    );

    drawText(
        385,
        105,
        "SEARCH",
        COLOR_TEXT
    );

    /*
     * Input placeholder
     */

    drawText(
        20,
        150,
        "PRESS X TO ENTER SEARCH",
        COLOR_DIM
    );

    drawText(
        20,
        180,
        "SEARCH RESULTS WILL APPEAR HERE",
        COLOR_DIM
    );
}

/* --------------------------------------------------------- */
/* Library                                                      */
/* --------------------------------------------------------- */

void drawLibrary(void)
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
        20,
        58,
        "LIBRARY",
        COLOR_TEXT
    );

    drawBorder(
        20,
        90,
        440,
        90,
        1,
        COLOR_BORDER
    );

    drawTextLarge(
        145,
        110,
        "LIBRARY",
        COLOR_TEXT
    );

    drawText(
        150,
        140,
        "NO SAVED VIDEOS",
        COLOR_DIM
    );

    drawText(
        150,
        160,
        "VIDEOS WILL APPEAR HERE",
        COLOR_DIM
    );
}

/* --------------------------------------------------------- */
/* Settings                                                     */
/* --------------------------------------------------------- */

void drawSettings(int selectedSetting)
{
    const char *items[] = {
        "VIDEO QUALITY",
        "AUTOPLAY",
        "THEME",
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

    drawTextLarge(
        20,
        58,
        "SETTINGS",
        COLOR_TEXT
    );

    for (i = 0; i < 4; i++)
    {
        int y = 90 + i * 38;

        if (i == selectedSetting)
        {
            drawRect(
                20,
                y - 7,
                300,
                28,
                COLOR_SELECT
            );

            drawRect(
                20,
                y - 7,
                3,
                28,
                COLOR_RED
            );
        }

        drawText(
            35,
            y,
            items[i],
            i == selectedSetting
                ? COLOR_TEXT
                : COLOR_DIM
        );
    }

    drawText(
        350,
        100,
        "PSP",
        COLOR_DIM
    );

    drawText(
        350,
        115,
        "YOUTUBE",
        COLOR_DIM
    );

    drawText(
        350,
        145,
        "VERSION",
        COLOR_DIM
    );

    drawText(
        350,
        160,
        "0.2V",
        COLOR_TEXT
    );
}

/* --------------------------------------------------------- */
/* Video details                                                */
/* --------------------------------------------------------- */

void drawVideoDetails(int selectedVideo)
{
    /*
     * Safety for array access.
     */

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

    /*
     * Video preview
     */

    drawThumbnail(
        20,
        60,
        250,
        120,
        0
    );

    /*
     * Information
     */

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

    /*
     * Play button
     */

    drawRect(
        290,
        140,
        120,
        32,
        COLOR_RED
    );

    drawText(
        32