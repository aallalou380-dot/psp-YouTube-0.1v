#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>

#include <string.h>

PSP_MODULE_INFO("PSP YouTube", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUF_WIDTH     512

static unsigned int __attribute__((aligned(16))) list[262144];

/* ---------------------------------------------------------
   Colors
   --------------------------------------------------------- */

#define RGB(r,g,b) \
    (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))

#define COLOR_BG       RGB(18,18,18)
#define COLOR_PANEL    RGB(25,25,25)
#define COLOR_PANEL2   RGB(38,38,38)
#define COLOR_TEXT     RGB(240,240,240)
#define COLOR_DIM      RGB(150,150,150)
#define COLOR_RED      RGB(230,35,35)
#define COLOR_SELECT   RGB(55,55,55)

/* ---------------------------------------------------------
   Exit callback
   --------------------------------------------------------- */

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

/* ---------------------------------------------------------
   Graphics initialization
   --------------------------------------------------------- */

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

/* ---------------------------------------------------------
   Rectangle
   --------------------------------------------------------- */

typedef struct
{
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
    Vertex *vertices;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    sceGuBegin(
        GU_SPRITES,
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D
    );

    vertices = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    vertices[0].x = (float)x;
    vertices[0].y = (float)y;
    vertices[0].z = 0.0f;

    vertices[1].x = (float)(x + width);
    vertices[1].y = (float)(y + height);
    vertices[1].z = 0.0f;

    sceGuVertex(
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        vertices
    );

    sceGuEnd();
}

/* ---------------------------------------------------------
   Border
   --------------------------------------------------------- */

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

/* ---------------------------------------------------------
   Simple text
   --------------------------------------------------------- */

void drawText(
    int x,
    int y,
    const char *text,
    unsigned int color
)
{
    /*
       Temporary text representation.

       We will replace this later with
       a proper PSP font renderer.
    */

    int i = 0;

    while (text[i] != '\0')
    {
        int px = x + i * 7;

        /*
           Small block representing a character.
           This keeps the first graphical build simple.
        */

        drawRect(
            px,
            y,
            5,
            7,
            color
        );

        i++;
    }
}

/* ---------------------------------------------------------
   Video information
   --------------------------------------------------------- */

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

/* ---------------------------------------------------------
   Thumbnail
   --------------------------------------------------------- */

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
        COLOR_PANEL2
    );

    /*
       Fake thumbnail content.

       Real YouTube thumbnails will be added later.
    */

    drawRect(
        x + 8,
        y + 8,
        width - 16,
        4,
        COLOR_DIM
    );

    drawRect(
        x + 8,
        y + height - 12,
        width - 16,
        4,
        COLOR_DIM
    );

    /* Play button */

    drawRect(
        x + width / 2 - 3,
        y + height / 2 - 15,
        6,
        30,
        COLOR_RED
    );

    drawRect(
        x + width / 2 + 3,
        y + height / 2 - 9,
        6,
        18,
        COLOR_RED
    );

    drawRect(
        x + width / 2 + 9,
        y + height / 2 - 3,
        6,
        6,
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
            COLOR_RED
        );
    }
}

/* ---------------------------------------------------------
   Sidebar
   --------------------------------------------------------- */

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
    int y = 72;

    drawRect(
        0,
        0,
        125,
        SCREEN_HEIGHT,
        COLOR_PANEL
    );

    /*
       Logo
    */

    drawRect(
        15,
        14,
        28,
        20,
        COLOR_RED
    );

    drawText(
        50,
        18,
        "PSP",
        COLOR_TEXT
    );

    drawText(
        15,
        43,
        "YOUTUBE",
        COLOR_DIM
    );

    for (i = 0; i < 5; i++)
    {
        if (i == selectedMenu)
        {
            drawRect(
                8,
                y - 7,
                108,
                24,
                COLOR_SELECT
            );

            drawRect(
                8,
                y - 7,
                4,
                24,
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

/* ---------------------------------------------------------
   Home
   --------------------------------------------------------- */

void drawHome(
    int selectedMenu,
    int selectedVideo
)
{
    int cardWidth = 150;
    int cardHeight = 76;

    int x1 = 145;
    int x2 = 315;

    int y1 = 76;
    int y2 = 180;

    drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        COLOR_BG
    );

    drawSidebar(selectedMenu);

    /*
       Header
    */

    drawRect(
        125,
        0,
        SCREEN_WIDTH - 125,
        45,
        COLOR_PANEL
    );

    drawText(
        145,
        16,
        "HOME",
        COLOR_TEXT
    );

    /*
       Search icon
    */

    drawBorder(
        420,
        10,
        13,
        13,
        2,
        COLOR_TEXT
    );

    drawRect(
        431,
        22,
        7,
        2,
        COLOR_TEXT
    );

    /*
       Section
    */

    drawText(
        145,
        57,
        "RECOMMENDED",
        COLOR_TEXT
    );

    /*
       Video 0
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
       Video 1
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
       Video 2
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
       Video 3
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

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void)
{
    SceCtrlData pad;
    SceCtrlData oldPad;

    int selectedMenu = 0;
    int selectedVideo = 0;

    memset(
        &oldPad,
        0,
        sizeof(oldPad)
    );

    SetupCallbacks();

    initGraphics();

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
            if (selectedVideo == 0)
                selectedVideo = 2;
            else if (selectedVideo == 1)
                selectedVideo = 3;
        }

        /*
           UP
        */

        if ((pad.Buttons & PSP_CTRL_UP) &&
            !(oldPad.Buttons & PSP_CTRL_UP))
        {
            if (selectedVideo == 2)
                selectedVideo = 0;
            else if (selectedVideo == 3)
                selectedVideo = 1;
        }

        /*
           RIGHT
        */

        if ((pad.Buttons & PSP_CTRL_RIGHT) &&
            !(oldPad.Buttons & PSP_CTRL_RIGHT))
        {
            if (selectedVideo == 0)
                selectedVideo = 1;
            else if (selectedVideo == 2)
                selectedVideo = 3;
        }

        /*
           LEFT
        */

        if ((pad.Buttons & PSP_CTRL_LEFT) &&
            !(oldPad.Buttons & PSP_CTRL_LEFT))
        {
            if (selectedVideo == 1)
                selectedVideo = 0;
            else if (selectedVideo == 3)
                selectedVideo = 2;
        }

        /*
           X = SELECT
        */

        if ((pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldPad.Buttons & PSP_CTRL_CROSS))
        {
            /*
               Video opening will be implemented later.
            */
        }

        /*
           O = BACK

           No action on Home.
        */

        /*
           HOME

           Not handled manually.
           PSP exit callback handles it.
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