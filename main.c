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

/* =========================================================
   COLORS
   ========================================================= */

#define RGB(r,g,b) \
    (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))

#define COLOR_BG       RGB(16,16,16)
#define COLOR_PANEL    RGB(23,23,23)
#define COLOR_CARD     RGB(35,35,35)
#define COLOR_CARD2    RGB(43,43,43)
#define COLOR_TEXT     RGB(245,245,245)
#define COLOR_DIM      RGB(150,150,150)
#define COLOR_RED      RGB(230,35,35)
#define COLOR_SELECT   RGB(52,52,52)
#define COLOR_BORDER   RGB(90,90,90)

/* =========================================================
   EXIT CALLBACK
   ========================================================= */

int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp)
{
    int cbid;

    cbid = sceKernelCreateCallback(
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
    int thid;

    thid = sceKernelCreateThread(
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

/* =========================================================
   GRAPHICS
   ========================================================= */

void initGraphics(void)
{
    sceGuInit();

    sceGuStart(
        GU_DIRECT,
        list
    );

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

/* =========================================================
   VERTEX
   ========================================================= */

typedef struct
{
    float x;
    float y;
    float z;
} Vertex;

/* =========================================================
   RECTANGLE
   ========================================================= */

void drawRect(
    int x,
    int y,
    int width,
    int height,
    unsigned int color
)
{
    Vertex *v;

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
   5x7 BITMAP FONT
   =========================================================
   
   Simple built-in Latin font.
   This replaces the old fake character blocks.
   
   Arabic support will be added later.
   ========================================================= */

static const unsigned char font5x7[96][5] =
{
    /* 32 SPACE */
    {0,0,0,0,0},

    /* ! */
    {0x04,0x04,0x04,0x00,0x04},

    /* " */
    {0x0A,0x0A,0x00,0x00,0x00},

    /* # */
    {0x0A,0x1F,0x0A,0x1F,0x0A},

    /* $ */
    {0x04,0x0E,0x14,0x0E,0x05},

    /* % */
    {0x19,0x19,0x02,0x13,0x13},

    /* & */
    {0x0C,0x12,0x0C,0x15,0x0A},

    /* ' */
    {0x04,0x04,0x00,0x00,0x00},

    /* ( */
    {0x02,0x04,0x08,0x04,0x02},

    /* ) */
    {0x08,0x04,0x02,0x04,0x08},

    /* * */
    {0x00,0x0A,0x04,0x0A,0x00},

    /* + */
    {0x00,0x04,0x0E,0x04,0x00},

    /* , */
    {0x00,0x00,0x00,0x04,0x08},

    /* - */
    {0x00,0x00,0x0E,0x00,0x00},

    /* . */
    {0x00,0x00,0x00,0x00,0x04},

    /* / */
    {0x01,0x02,0x04,0x08,0x10},

    /* 0 */
    {0x0E,0x11,0x13,0x15,0x0E},

    /* 1 */
    {0x04,0x0C,0x04,0x04,0x0E},

    /* 2 */
    {0x0E,0x11,0x02,0x04,0x1F},

    /* 3 */
    {0x1E,0x01,0x06,0x01,0x1E},

    /* 4 */
    {0x02,0x06,0x0A,0x1F,0x02},

    /* 5 */
    {0x1F,0x10,0x1E,0x01,0x1E},

    /* 6 */
    {0x06,0x08,0x1E,0x11,0x0E},

    /* 7 */
    {0x1F,0x01,0x02,0x04,0x08},

    /* 8 */
    {0x0E,0x11,0x0E,0x11,0x0E},

    /* 9 */
    {0x0E,0x11,0x0F,0x01,0x0E},

    /* : */
    {0x00,0x04,0x00,0x04,0x00},

    /* ; */
    {0x00,0x04,0x00,0x04,0x08},

    /* < */
    {0x02,0x04,0x08,0x04,0x02},

    /* = */
    {0x00,0x0E,0x00,0x0E,0x00},

    /* > */
    {0x08,0x04,0x02,0x04,0x08},

    /* ? */
    {0x0E,0x11,0x02,0x00,0x02},

    /* @ */
    {0x0E,0x11,0x17,0x10,0x0E},

    /* A */
    {0x0E,0x11,0x1F,0x11,0x11},

    /* B */
    {0x1E,0x11,0x1E,0x11,0x1E},

    /* C */
    {0x0E,0x11,0x10,0x11,0x0E},

    /* D */
    {0x1E,0x11,0x11,0x11,0x1E},

    /* E */
    {0x1F,0x10,0x1E,0x10,0x1F},

    /* F */
    {0x1F,0x10,0x1E,0x10,0x10},

    /* G */
    {0x0E,0x10,0x17,0x11,0x0E},

    /* H */
    {0x11,0x11,0x1F,0x11,0x11},

    /* I */
    {0x0E,0x04,0x04,0x04,0x0E},

    /* J */
    {0x07,0x02,0x02,0x12,0x0C},

    /* K */
    {0x11,0x12,0x1C,0x12,0x11},

    /* L */
    {0x10,0x10,0x10,0x10,0x1F},

    /* M */
    {0x11,0x1B,0x15,0x11,0x11},

    /* N */
    {0x11,0x19,0x15,0x13,0x11},

    /* O */
    {0x0E,0x11,0x11,0x11,0x0E},

    /* P */
    {0x1E,0x11,0x1E,0x10,0x10},

    /* Q */
    {0x0E,0x11,0x11,0x15,0x0E},

    /* R */
    {0x1E,0x11,0x1E,0x12,0x11},

    /* S */
    {0x0F,0x10,0x0E,0x01,0x1E},

    /* T */
    {0x1F,0x04,0x04,0x04,0x04},

    /* U */
    {0x11,0x11,0x11,0x11,0x0E},

    /* V */
    {0x11,0x11,0x11,0x0A,0x04},

    /* W */
    {0x11,0x11,0x15,0x1B,0x11},

    /* X */
    {0x11,0x0A,0x04,0x0A,0x11},

    /* Y */
    {0x11,0x0A,0x04,0x04,0x04},

    /* Z */
    {0x1F,0x02,0x04,0x08,0x1F},

    /* [ */
    {0x0E,0x08,0x08,0x08,0x0E},

    /* \ */
    {0x10,0x08,0x04,0x02,0x01},

    /* ] */
    {0x0E,0x02,0x02,0x02,0x0E},

    /* ^ */
    {0x04,0x0A,0x11,0x00,0x00},

    /* _ */
    {0x00,0x00,0x00,0x00,0x1F},

    /* ` */
    {0x08,0x04,0x00,0x00,0x00},

    /* a */
    {0x00,0x0E,0x01,0x0F,0x0F},

    /* b */
    {0x10,0x10,0x1E,0x11,0x1E},

    /* c */
    {0x00,0x0E,0x10,0x10,0x0E},

    /* d */
    {0x01,0x01,0x0F,0x11,0x0F},

    /* e */
    {0x00,0x0E,0x11,0x1F,0x0E},

    /* f */
    {0x06,0x08,0x1E,0x08,0x08},

    /* g */
    {0x00,0x0F,0x11,0x0F,0x01},

    /* h */
    {0x10,0x10,0x1E,0x11,0x11},

    /* i */
    {0x04,0x00,0x0C,0x04,0x0E},

    /* j */
    {0x02,0x00,0x06,0x02,0x1C},

    /* k */
    {0x10,0x12,0x1C,0x12,0x11},

    /* l */
    {0x0C,0x04,0x04,0x04,0x0E},

    /* m */
    {0x00,0x1A,0x15,0x15,0x15},

    /* n */
    {0x00,0x1E,0x11,0x11,0x11},

    /* o */
    {0x00,0x0E,0x11,0x11,0x0E},

    /* p */
    {0x00,0x1E,0x11,0x1E,0x10},

    /* q */
    {0x00,0x0F,0x11,0x0F,0x01},

    /* r */
    {0x00,0x16,0x19,0x10,0x10},

    /* s */
    {0x00,0x0F,0x18,0x07,0x1E},

    /* t */
    {0x08,0x1E,0x08,0x08,0x06},

    /* u */
    {0x00,0x11,0x11,0x13,0x0D},

    /* v */
    {0x00,0x11,0x11,0x0A,0x04},

    /* w */
    {0x00,0x11,0x15,0x15,0x0A},

    /* x */
    {0x00,0x11,0x0A,0x04,0x0A},

    /* y */
    {0x00,0x11,0x0F,0x01,0x0E},

    /* z */
    {0x00,0x1F,0x02,0x08,0x1F},

    /* { */
    {0x02,0x04,0x04,0x04,0x02},

    /* | */
    {0x04,0x04,0x04,0x04,0x04},

    /* } */
    {0x08,0x04,0x04,0x04,0x08},

    /* ~ */
    {0x00,0x09,0x16,0x00,0x00}
};

/* =========================================================
   TEXT
   ========================================================= */

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
    unsigned char bits;

    if (c < 32 || c > 127)
        c = '?';

    for (row = 0; row < 7; row++)
    {
        if (row >= 5)
            continue;

        bits = font5x7[(unsigned char)c - 32][row];

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

/* =========================================================
   VIDEO DATA
   ========================================================= */

typedef struct
{
    const char *title;
    const char *channel;
} Video;

static Video videos[] =
{
    {
        "Welcome to PSP YouTube",
        "PSP Channel"
    },

    {
        "Latest Gaming Videos",
        "Gaming"
    },

    {
        "PSP Homebrew News",
        "PSP Dev"
    },

    {
        "Retro Games",
        "Retro"
    },

    {
        "Technology",
        "Tech"
    },

    {
        "New PSP Projects",
        "Homebrew"
    }
};

#define VIDEO_COUNT 6

/* =========================================================
   THUMBNAIL
   ========================================================= */

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

    /*
       Thumbnail center
    */

    drawRect(
        x + 1,
        y + 1,
        width - 2,
        height - 2,
        COLOR_CARD2
    );

    /*
       Simple image lines
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
       Play icon
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

    /*
       Selection border
    */

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

/* =========================================================
   SIDEBAR
   ========================================================= */

void drawSidebar(int selectedMenu)
{
    const char *items[] =
    {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS"
    };

    int i;
    int y = 75;

    /*
       Sidebar
    */

    drawRect(
        0,
        0,
        125,
        SCREEN_HEIGHT,
        COLOR_PANEL
    );

    /*
       Vertical separator
    */

    drawRect(
        124,
        0,
        1,
        SCREEN_HEIGHT,
        COLOR_BORDER
    );

    /*
       Logo
    */

    drawRect(
        15,
        13,
        31,
        21,
        COLOR_RED
    );

    drawRect(
        28,
        18,
        8,
        11,
        COLOR_TEXT
    );

    drawTextLarge(
        52,
        15,
        "PSP",
        COLOR_TEXT
    );

    drawText(
        15,
        42,
        "YOUTUBE",
        COLOR_DIM
    );

    /*
       Menu
    */

    for (i = 0; i < 5; i++)
    {
        if (i == selectedMenu)
        {
            drawRect(
                8,
                y - 8,
                108,
                25,
                COLOR_SELECT
            );

            drawRect(
                8,
                y - 8,
                3,
                25,
                COLOR_RED
            );
        }

        drawText(
            20,
            y,
            items[i],
            i == selectedMenu
                ? COLOR_TEXT
                : COLOR_DIM
        );

        y += 32;
    }
}

/* =========================================================
   HEADER
   ========================================================= */

void drawHeader(
    int selectedMenu
)
{
    const char *titles[] =
    {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS"
    };

    drawRect(
        125,
        0,
        SCREEN_WIDTH - 125,
        45,
        COLOR_PANEL
    );

    drawTextLarge(
        145,
        13,
        titles[selectedMenu],
        COLOR_TEXT
    );

    /*
       Search box
    */

    drawBorder(
        345,
        10,
        82,
        22,
        1,
        COLOR_BORDER
    );

    drawText(
        355,
        17,
        "SEARCH",
        COLOR_DIM
    );

    /*
       Search icon
    */

    drawBorder(
        438,
        10,
        12,
        12,
        2,
        COLOR_TEXT
    );

    drawRect(
        449,
        21,
        6,
        2,
        COLOR_TEXT
    );
}

/* =========================================================
   HOME
   ========================================================= */

void drawHome(
    int selectedMenu,
    int selectedVideo
)
{
    int cardWidth = 150;
    int cardHeight = 70;

    int x1 = 145;
    int x2 = 315;

    int y1 = 70;
    int y2 = 171;

    /*
       Background
    */

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    /*
       Sidebar
    */

    drawSidebar(
        selectedMenu
    );

    /*
       Header
    */

    drawHeader(
        selectedMenu
    );

    /*
       Section
    */

    drawTextLarge(
        145,
        53,
        "RECOMMENDED",
        COLOR_TEXT
    );

    /*
       VIDEO 0
    */

    drawThumbnail(
        x1,
        y1,
        cardWidth,
        cardHeight,
        selectedVideo == 0
    );

    drawText(
        x1,
        y1 + cardHeight + 7,
        videos[0].title,
        COLOR_TEXT
    );

    drawText(
        x1,
        y1 + cardHeight + 18,
        videos[0].channel,
        COLOR_DIM
    );

    /*
       VIDEO 1
    */

    drawThumbnail(
        x2,
        y1,
        cardWidth,
        cardHeight,
        selectedVideo == 1
    );

    drawText(
        x2,
        y1 + cardHeight + 7,
        videos[1].title,
        COLOR_TEXT
    );

    drawText(
        x2,
        y1 + cardHeight + 18,
        videos[1].channel,
        COLOR_DIM
    );

    /*
       VIDEO 2
    */

    drawThumbnail(
        x1,
        y2,
        cardWidth,
        cardHeight,
        selectedVideo == 2
    );

    drawText(
        x1,
        y2 + cardHeight + 7,
        videos[2].title,
        COLOR_TEXT
    );

    drawText(
        x1,
        y2 + cardHeight + 18,
        videos[2].channel,
        COLOR_DIM
    );

    /*
       VIDEO 3
    */

    drawThumbnail(
        x2,
        y2,
        cardWidth,
        cardHeight,
        selectedVideo == 3
    );

    drawText(
        x2,
        y2 + cardHeight + 7,
        videos[3].title,
        COLOR_TEXT
    );

    drawText(
        x2,
        y2 + cardHeight + 18,
        videos[3].channel,
        COLOR_DIM
    );
}

/* =========================================================
   OTHER PAGES PLACEHOLDER
   ========================================================= */

void drawPlaceholder(
    int selectedMenu
)
{
    const char *titles[] =
    {
        "HOME",
        "TRENDING",
        "SEARCH",
        "LIBRARY",
        "SETTINGS"
    };

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawSidebar(
        selectedMenu
    );

    drawHeader(
        selectedMenu
    );

    drawTextLarge(
        160,
        105,
        titles[selectedMenu],
        COLOR_TEXT
    );

    drawText(
        160,
        135,
        "COMING SOON",
        COLOR_DIM
    );
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    SceCtrlData pad;
    SceCtrlData oldPad;

    int selectedMenu = 0;
    int selectedVideo = 0;

    /*
       Focus:
       0 = sidebar
       1 = video grid
    */

    int focus = 0;

    memset(
        &oldPad,
        0,
        sizeof(oldPad)
    );

    SetupCallbacks();

    initGraphics();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_DIGITAL
    );

    while (1)
    {
        sceCtrlReadBufferPositive(
            &pad,
            1
        );

        /* =================================================
           SIDEBAR FOCUS
           ================================================= */

        if (focus == 0)
        {
            /*
               DOWN
            */

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(oldPad.Buttons & PSP_CTRL_DOWN))
            {
                if (selectedMenu < 4)
                    selectedMenu++;
            }

            /*
               UP
            */

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(oldPad.Buttons & PSP_CTRL_UP))
            {
                if (selectedMenu > 0)
                    selectedMenu--;
            }

            /*
               RIGHT -> video area
            */

            if ((pad.Buttons & PSP_CTRL_RIGHT) &&
                !(oldPad.Buttons & PSP_CTRL_RIGHT))
            {
                if (selectedMenu == 0)
                    focus = 1;
            }

            /*
               X
            */

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldPad.Buttons & PSP_CTRL_CROSS))
            {
                if (selectedMenu != 0)
                {
                    /*
                       Other pages will be implemented later.
                    */
                }
            }
        }

        /* =================================================
           VIDEO FOCUS
           ================================================= */

        else
        {
            /*
               LEFT
            */

            if ((pad.Buttons & PSP_CTRL_LEFT) &&
                !(oldPad.Buttons & PSP_CTR