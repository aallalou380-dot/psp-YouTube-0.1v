#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("PSP YouTube", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

/* =========================================================
   PSP SCREEN
   ========================================================= */

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272

#define BUF_WIDTH 512

static unsigned int __attribute__((aligned(16))) list[262144];

/* =========================================================
   COLORS
   ========================================================= */

#define RGB(r,g,b) ((unsigned int)(0xFF000000 | ((b) << 16) | ((g) << 8) | (r)))

#define COLOR_BG        RGB(18,18,18)
#define COLOR_PANEL     RGB(25,25,25)
#define COLOR_PANEL2    RGB(32,32,32)
#define COLOR_TEXT      RGB(235,235,235)
#define COLOR_TEXT_DIM  RGB(150,150,150)
#define COLOR_RED       RGB(230,35,35)
#define COLOR_WHITE     RGB(255,255,255)
#define COLOR_BLACK     RGB(0,0,0)
#define COLOR_SELECT    RGB(55,55,55)
#define COLOR_BORDER    RGB(255,45,45)

/* =========================================================
   CALLBACK
   ========================================================= */

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
        sceKernelStartThread(thid, 0, 0);

    return thid;
}

/* =========================================================
   GU INITIALIZATION
   ========================================================= */

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
    sceGuDisable(GU_LIGHTING);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* =========================================================
   DRAW RECTANGLE
   ========================================================= */

void drawRect(
    int x,
    int y,
    int width,
    int height,
    unsigned int color
)
{
    sceGuDisable(GU_TEXTURE_2D);

    sceGuColor(color);

    sceGuBegin(GU_SPRITES, GU_VERTEX_32BITF | GU_TRANSFORM_2D);

    sceGuVertex(
        (float)x,
        (float)y,
        0.0f
    );

    sceGuVertex(
        (float)(x + width),
        (float)(y + height),
        0.0f
    );

    sceGuEnd();
}

/* =========================================================
   BORDER
   ========================================================= */

void drawBorder(
    int x,
    int y,
    int width,
    int height,
    int thickness,
    unsigned int color
)
{
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

/* =========================================================
   SIMPLE PIXEL FONT
   ========================================================= */

static const unsigned char font5x7[96][7] =
{
    {0,0,0,0,0,0,0},
    {4,4,4,4,4,0,4},
    {10,10,0,0,0,0,0},
    {10,31,10,10,31,10,0},
    {4,15,20,14,5,30,4},
    {24,25,2,4,8,19,3},
    {12,18,20,8,21,18,13},
    {6,4,0,0,0,0,0},
    {2,4,8,8,8,4,2},
    {8,4,2,2,2,4,8},
    {0,4,21,14,21,4,0},
    {0,4,4,31,4,4,0},
    {0,0,0,0,6,4,8},
    {0,0,0,31,0,0,0},
    {0,0,0,0,0,6,6},
    {0,1,2,4,8,16,0},

    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {31,2,4,2,1,17,14},
    {2,6,10,18,31,2,2},
    {31,16,30,1,1,17,14},
    {6,8,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,2,12},

    {0,0,6,6,0,6,6},
    {0,0,6,6,0,4,8},
    {2,4,8,16,8,4,2},
    {0,0,31,0,31,0,0},
    {8,4,2,1,2,4,8},
    {14,17,1,2,4,0,4},

    {14,17,1,13,21,21,14},
    {14,17,17,31,17,17,17},
    {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},
    {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},
    {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},
    {17,17,17,21,21,27,17},
    {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31},

    {14,8,8,8,8,8,14},
    {16,8,4,2,1,0,0},
    {14,2,2,2,2,2,14},
    {4,10,17,0,0,0,0},
    {0,0,0,0,0,0,31},
    {8,4,2,0,0,0,0},

    {0,0,14,1,15,17,15},
    {16,16,22,25,17,17,30},
    {0,0,14,17,16,17,14},
    {1,1,13,19,17,17,15},
    {0,0,14,17,31,16,14},
    {6,9,8,28,8,8,8},
    {0,0,15,17,17,15,1,14},
    {16,16,22,25,17,17,17},
    {4,0,12,4,4,4,14},
    {2,0,6,2,2,18,12},
    {16,16,18,20,24,20,18},
    {12,4,4,4,4,4,14},
    {0,0,26,21,21,17,17},
    {0,0,22,25,17,17,17},
    {0,0,14,17,17,17,14},
    {0,0,30,17,17,30,16},
    {0,0,15,17,17,15,1,1},
    {0,0,22,25,16,16,16},
    {0,0,15,16,14,1,30},
    {8,8,28,8,8,9,6},
    {0,0,17,17,17,19,13},
    {0,0,17,17,17,10,4},
    {0,0,17,17,21,21,10},
    {0,0,17,10,4,10,17},
    {0,0,17,17,17,15,1,14},
    {0,0,31,2,4,8,31},
    {3,4,4,8,4,4,3},
    {4,4,4,4,4,4,4},
    {24,4,4,2,4,4,24},
    {8,21,2,0,0,0,0},
    {0,0,0,0,0,0,0}
};

/* =========================================================
   DRAW CHARACTER
   ========================================================= */

void drawChar(
    int x,
    int y,
    char c,
    int scale,
    unsigned int color
)
{
    if (c < 32 || c > 127)
        return;

    const unsigned char *glyph =
        font5x7[c - 32];

    for (int row = 0; row < 7; row++)
    {
        unsigned char bits = glyph[row];

        for (int col = 0; col < 5; col++)
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

/* =========================================================
   DRAW TEXT
   ========================================================= */

void drawText(
    int x,
    int y,
    const char *text,
    int scale,
    unsigned int color
)
{
    int pos = x;

    while (*text)
    {
        if (*text == '\n')
        {
            y += 8 * scale;
            pos = x;
        }
        else
        {
            drawChar(
                pos,
                y,
                *text,
                scale,
                color
            );

            pos += 6 * scale;
        }

        text++;
    }
}

/* =========================================================
   VIDEO DATA
   ========================================================= */

typedef struct
{
    const char *title;
    const char *channel;
} Video;

Video videos[] =
{
    {"Welcome to PSP YouTube", "PSP Channel"},
    {"Latest Gaming Videos",   "Gaming"},
    {"PSP Homebrew News",      "PSP Dev"},
    {"Retro Games",            "Retro"},
    {"Technology",             "Tech"},
    {"New PSP Projects",       "Homebrew"}
};

#define VIDEO_COUNT 6

/* =========================================================
   DRAW THUMBNAIL
   ========================================================= */

void drawThumbnail(
    int x,
    int y,
    int width,
    int height,
    int index,
    int selected
)
{
    /*
       Placeholder thumbnail.

       Later this area will contain
       real YouTube thumbnail images.
    */

    drawRect(
        x,
        y,
        width,
        height,
        COLOR_PANEL2
    );

    /* fake image pattern */
    drawRect(
        x + 5,
        y + 5,
        width - 10,
        4,
        COLOR_TEXT_DIM
    );

    drawRect(
        x + 5,
        y + height - 9,
        width - 10,
        4,
        COLOR_TEXT_DIM
    );

    /* Play symbol */
    drawRect(
        x + width / 2 - 3,
        y + height / 2 - 18,
        6,
        36,
        COLOR_RED
    );

    drawRect(
        x + width / 2 + 3,
        y + height / 2 - 12,
        6,
        24,
        COLOR_RED
    );

    drawRect(
        x + width / 2 + 9,
        y + height / 2 - 6,
        6,
        12,
        COLOR_RED
    );

    if (selected)
    {
        drawBorder(
            x - 2,
            y - 2,
            width + 4,
            height + 4,
            3,
            COLOR_BORDER
        );
    }
}

/* =========================================================
   SIDEBAR
   ========================================================= */

void drawSidebar(int selectedMenu)
{
    drawRect(
        0,
        0,
        125,
        SCREEN_HEIGHT,
        COLOR_PANEL
    );

    /* Logo */
    drawRect(
        15,
        14,
        28,
        20,
        COLOR_RED
    );

    drawText(
        48,
        17,
        "PSP",
        2,
        COLOR_WHITE
    );

    drawText(
        15,
        43,
        "YOUTUBE",
        1,
        COLOR_TEXT_DIM
    );

    const char *items[] =
    {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS"
    };

    int y = 72;

    for (int i = 0; i < 5; i++)
    {
        if (i == selectedMenu)
        {
            drawRect(
                8,
                y - 6,
                109,
                25,
                COLOR_SELECT
            );

            drawRect(
                8,
                y - 6,
                4,
                25,
                COLOR_RED
            );
        }

        drawText(
            20,
            y,
            items[i],
            1,
            i == selectedMenu
                ? COLOR_WHITE
                : COLOR_TEXT_DIM
        );

        y += 32;
    }
}

/* =========================================================
   HOME SCREEN
   ========================================================= */

void drawHome(
    int selectedMenu,
    int selectedVideo
)
{
    /* Background */
    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    /* Sidebar */
    drawSidebar(selectedMenu);

    /* Top bar */
    drawRect(
        125,
        0,
        SCREEN_WIDTH - 125,
        45,
        COLOR_PANEL
    );

    drawText(
        145,
        14,
        "HOME",
        2,
        COLOR_WHITE
    );

    /* Search icon */
    drawBorder(
        418,
        12,
        14,
        14,
        2,
        COLOR_TEXT
    );

    drawRect(
        430,
        25,
        8,
        2,
        COLOR_TEXT
    );

    /* Content title */
    drawText(
        145,
        57,
        "RECOMMENDED",
        1,
        COLOR_TEXT
    );

    /*
       Two-column layout
    */

    int cardWidth = 150;
    int cardHeight = 78;

    int x1 = 145;
    int x2 = 315;

    int y1 = 76;
    int y2 = 180;

    /* Video 0 */
    drawThumbnail(
        x1,
        y1,
        cardWidth,
        cardHeight,
        0,
        selectedVideo == 0
    );

    drawText(
        x1,
        y1 + cardHeight + 7,
        videos[0].title,
        1,
        COLOR_WHITE
    );

    drawText(
        x1,
        y1 + cardHeight + 18,
        videos[0].channel,
        1,
        COLOR_TEXT_DIM
    );

    /* Video 1 */
    drawThumbnail(
        x2,
        y1,
        cardWidth,
        cardHeight,
        1,
        selectedVideo == 1
    );

    drawText(
        x2,
        y1 + cardHeight + 7,
        videos[1].title,
        1,
        COLOR_WHITE
    );

    drawText(
        x2,
        y1 + cardHeight + 18,
        videos[1].channel,
        1,
        COLOR_TEXT_DIM
    );

    /* Video 2 */
    drawThumbnail(
        x1,
        y2,
        cardWidth,
        cardHeight,
        2,
        selectedVideo == 2
    );

    drawText(
        x1,
        y2 + cardHeight + 7,
        videos[2].title,
        1,
        COLOR_WHITE
    );

    drawText(
        x1,
        y2 + cardHeight + 18,
        videos[2].channel,
        1,
        COLOR_TEXT_DIM
    );

    /* Video 3 */
    drawThumbnail(
        x2,
        y2,
        cardWidth,
        cardHeight,
        3,
        selectedVideo == 3
    );

    drawText(
        x2,
        y2 + cardHeight + 7,
        videos[3].title,
        1,
        COLOR_WHITE
    );

    drawText(
        x2,
        y2 + cardHeight + 18,
        videos[3].channel,
        1,
        COLOR_TEXT_DIM
    );

    /* Bottom hint */
    drawText(
        145,
        258,
        "X SELECT    O BACK",
        1,
        COLOR_TEXT_DIM
    );
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    SetupCallbacks();

    initGraphics();

    SceCtrlData pad;
    SceCtrlData oldPad;

    memset(
        &oldPad,
        0,
        sizeof(oldPad)
    );

    int selectedMenu = 0;
    int selectedVideo = 0;

    while (1)
    {
        sceCtrlReadBufferPositive(
            &pad,
            1
        );

        /*
           DOWN
        */
        if ((pad.Buttons & PSP_CTRL_DOWN) &&
            !(oldPad.Buttons & PSP_CTRL_DOWN))
        {
            if (selectedVideo < 3)
                selectedVideo++;
        }

        /*
           UP
        */
        if ((pad.Buttons & PSP_CTRL_UP) &&
            !(oldPad.Buttons & PSP_CTRL_UP))
        {
            if (selectedVideo > 0)
                selectedVideo--;
        }

        /*
           RIGHT
        */
        if ((pad.Buttons & PSP_CTRL_RIGHT) &&
            !(oldPad.Buttons & PSP_CTRL_RIGHT))
        {
            if (selectedVideo == 0 ||
                selectedVideo == 2)
            {
                selectedVideo++;
            }
        }

        /*
           LEFT
        */
        if ((pad.Buttons & PSP_CTRL_LEFT) &&
            !(oldPad.Buttons & PSP_CTRL_LEFT))
        {
            if (selectedVideo == 1 ||
                selectedVideo == 3)
            {
                selectedVideo--;
            }
        }

        /*
           X = SELECT
        */
        if ((pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldPad.Buttons & PSP_CTRL_CROSS))
        {
            /*
               Later:
               open selected YouTube video.
            */
        }

        /*
           O = BACK
        */
        if ((pad.Buttons & PSP_CTRL_CIRCLE) &&
            !(oldPad.Buttons & PSP_CTRL_CIRCLE))
        {
            /*
               Later:
               return to previous screen.
            */
        }

        /*
           HOME is NOT handled here.
           PSP's normal exit callback handles it.
        */

        sceGuStart(
            GU_DIRECT,
            list
        );

        drawHome(
            selectedMenu,
            selectedVideo
        );

        sceGuFinish();
        sceGuSync(
            0,
            0
        );

        sceDisplayWaitVblankStart();

        sceGuSwapBuffers();

        oldPad = pad;
    }

    sceGuTerm();

    sceKernelExitGame();

    return 0;
}