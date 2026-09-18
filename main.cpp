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

#define W 480
#define H 272
#define BW 512

static unsigned int __attribute__((aligned(16))) list[262144];
static int running = 1;
static intraFont *font = 0;

static const unsigned int BG=0xFF0B0D11, PANEL=0xFF151820, PANEL2=0xFF1D212B,
TEXT=0xFFF4F5F7, MUTED=0xFF9EA3AE, RED=0xFFFF1238, WHITE=0xFFFFFFFF,
LINE=0xFF303541, BLACK=0xFF08090C;

struct V { unsigned int color; short x,y,z; } __attribute__((packed));

static int exit_cb(int,int,void*) { running=0; sceKernelExitGame(); return 0; }
static int cb_thread(SceSize,void*) { int id=sceKernelCreateCallback("Exit",exit_cb,0); if(id>=0)sceKernelRegisterExitCallback(id); sceKernelSleepThreadCB(); return 0; }
static void callbacks(){ SceUID t=sceKernelCreateThread("cb",cb_thread,0x11,0xFA0,0,0); if(t>=0)sceKernelStartThread(t,0,0); }

static void rect(int x,int y,int w,int h,unsigned int c){
    if(w<=0||h<=0)return;
    V *v=(V*)sceGuGetMemory(2*sizeof(V));
    v[0].color=c; v[0].x=x;v[0].y=y;v[0].z=0;
    v[1].color=c; v[1].x=x+w;v[1].y=y+h;v[1].z=0;
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,v);
}
static void border(int x,int y,int w,int h,unsigned int c,int t){
    rect(x,y,w,t,c); rect(x,y+h-t,w,t,c); rect(x,y,t,h,c); rect(x+w-t,y,t,h,c);
}
static void line(int x1,int y1,int x2,int y2,unsigned int c){
    V *v=(V*)sceGuGetMemory(2*sizeof(V));
    v[0].color=c;v[0].x=x1;v[0].y=y1;v[0].z=0; v[1].color=c;v[1].x=x2;v[1].y=y2;v[1].z=0;
    sceGuDrawArray(GU_LINES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,v);
}
static void text(const char *s,float x,float y,unsigned int c,float scale=0.55f){
    if(!font||!s)return;
    intraFontSetStyle(font,scale,c,0,0,INTRAFONT_ALIGN_LEFT);
    intraFontPrint(font,x,y,s);
}
static void logo(){
    rect(18,14,48,32,RED); // YouTube mark
    V *v=(V*)sceGuGetMemory(3*sizeof(V));
    v[0].color=WHITE;v[0].x=37;v[0].y=22;v[0].z=0;
    v[1].color=WHITE;v[1].x=37;v[1].y=38;v[1].z=0;
    v[2].color=WHITE;v[2].x=50;v[2].y=30;v[2].z=0;
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,3,0,v);
}

static int page=0, side=0, menu=0, card=0, searchActive=0;
static char query[64]="";
static unsigned int oldButtons=0;
static const char *menus[]={"Home","Trending","Shorts","Library","Settings"};
static const char *titles[]={"Discover new videos","PSP homebrew highlights","Gaming Shorts","Your saved videos"};
static const char *channels[]={"PSP YouTube","PSP Community","YouTube","Library"};
static const char *views[]={"12K views","8.4K views","4.1K views","1.7K views"};

static void searchBox(){
    rect(86,13,285,34,PANEL2); border(86,13,285,34,LINE,1);
    rect(97,23,12,12,0xFF555B66); rect(102,33,3,6,0xFF555B66);
    if(query[0]) text(query,120,36,TEXT,0.58f); else text("Search YouTube",120,36,MUTED,0.58f);
    rect(385,13,42,34,RED); text("R",400,36,WHITE,0.62f);
}
static void header(){ logo(); text("YouTube",72,36,TEXT,0.72f); searchBox(); }
static void sidebar(){
    if(!side)return;
    rect(0,0,172,H,0xFF11141B); rect(0,0,172,60,PANEL); border(0,0,172,H,LINE,1);
    text("YouTube",22,36,TEXT,0.72f);
    for(int i=0;i<5;i++){
        int y=74+i*37;
        if(i==menu) { rect(12,y-22,148,31,RED); text(menus[i],30,y,TEXT,0.60f); }
        else text(menus[i],30,y,MUTED,0.60f);
    }
    text("L",145,250,MUTED,0.50f);
}
static void thumb(int x,int y,int w,int h,int n){
    unsigned int a[]={0xFF242A36,0xFF26353B,0xFF312A3D,0xFF27342A};
    rect(x,y,w,h,a[n%4]);
    rect(x+8,y+8,w-16,3,0xFF424A58);
    rect(x+12,y+h-20,w-24,5,0xFF3A424F);
    // stylized play mark
    V *v=(V*)sceGuGetMemory(3*sizeof(V));
    int cx=x+w/2,cy=y+h/2;
    v[0].color=RED;v[0].x=cx-8;v[0].y=cy-12;v[0].z=0;
    v[1].color=RED;v[1].x=cx-8;v[1].y=cy+12;v[1].z=0;
    v[2].color=RED;v[2].x=cx+13;v[2].y=cy;v[2].z=0;
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,3,0,v);
}
static void cardView(int i,int x,int y){
    int w=134,h=101;
    rect(x,y,w,h,PANEL); if(i==card) border(x-2,y-2,w+4,h+4,RED,2);
    thumb(x+7,y+7,w-14,56,i);
    text(titles[i],x+7,y+76,TEXT,0.43f);
    text(channels[i],x+7,y+90,MUTED,0.38f);
    text(views[i],x+84,y+90,MUTED,0.34f);
}
static void home(){
    header();
    text(query[0]?"Search results":"Recommended",18,69,TEXT,0.70f);
    int sx=18, sy=82;
    for(int i=0;i<4;i++) cardView(i,sx+(i%3)*150,sy+(i/3)*111);
    rect(18,214,444,38,PANEL); text("X",31,239,RED,0.58f); text("Open selected video",52,239,MUTED,0.48f);
    text("L  Menu",378,266,MUTED,0.40f);
    sidebar();
}
static void videoPage(){
    rect(0,0,W,H,BG);
    rect(18,14,444,126,BLACK); border(18,14,444,126,LINE,1);
    rect(32,28,416,98,0xFF171B23);
    V *v=(V*)sceGuGetMemory(3*sizeof(V));
    v[0].x=224;v[0].y=55;v[0].z=0;v[0].color=RED;v[1].x=224;v[1].y=99;v[1].z=0;v[1].color=RED;v[2].x=260;v[2].y=77;v[2].z=0;v[2].color=WHITE;
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D,3,0,v);
    text("Selected video",20,160,TEXT,0.72f);
    text(channels[card],20,182,MUTED,0.52f); text(views[card],20,201,MUTED,0.46f);
    text(titles[card],20,228,TEXT,0.70f);
    text("A modern PSP YouTube interface prototype.",20,250,MUTED,0.42f);
    text("O  Back",410,267,MUTED,0.40f);
}

// PSP OSK: uses the system utility dialog; R opens it. If unavailable, the app remains stable.
static int openOsk(){
    enum { N=1, L=64 };
    static unsigned short intext[N][L], outtext[N][L], desc[N][L];
    memset(intext,0,sizeof(intext)); memset(outtext,0,sizeof(outtext)); memset(desc,0,sizeof(desc));
    const char *q=query;
    for(int i=0;q[i]&&i<L-1;i++) intext[0][i]=(unsigned char)q[i];
    const char *d="Search";
    for(int i=0;d[i]&&i<L-1;i++) desc[0][i]=(unsigned char)d[i];

    SceUtilityOskData data[N];
    memset(data,0,sizeof(data));
    data[0].language=PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data[0].lines=1;
    data[0].unk_24=1;
    data[0].inputtype=PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data[0].desc=desc[0];
    data[0].intext=intext[0];
    data[0].outtextlength=L;
    data[0].outtextlimit=L-1;
    data[0].outtext=outtext[0];

    SceUtilityOskParams params;
    memset(&params,0,sizeof(params));
    params.base.size=sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,&params.base.buttonSwap);
    params.base.graphicsThread=17; params.base.accessThread=19; params.base.fontThread=18; params.base.soundThread=16;
    params.datacount=N; params.data=data;
    if(sceUtilityOskInitStart(&params)<0) return 0;

    int finished=0;
    while(!finished && running){
        int st=sceUtilityOskGetStatus();
        if(st==PSP_UTILITY_DIALOG_VISIBLE) sceUtilityOskUpdate(1);
        else if(st==PSP_UTILITY_DIALOG_QUIT) sceUtilityOskShutdownStart();
        else if(st==PSP_UTILITY_DIALOG_NONE) finished=1;
        sceDisplayWaitVblankStart();
    }
    if(data[0].result==PSP_UTILITY_OSK_RESULT_CHANGED){
        int n=0; while(n<L-1 && outtext[0][n]){ unsigned short ch=outtext[0][n]; query[n]=(ch<128)?(char)ch:'?'; n++; }
        query[n]=0;
    }
    return 1;
}

static void input(){
    SceCtrlData p; sceCtrlReadBufferPositive(&p,1); unsigned int b=p.Buttons, pressed=b&~oldButtons;
    if(searchActive)return;
    if(pressed&PSP_CTRL_LTRIGGER){ side=!side; return; }
    if(side){
        if(pressed&PSP_CTRL_UP){menu=(menu+4)%5;}
        if(pressed&PSP_CTRL_DOWN){menu=(menu+1)%5;}
        if(pressed&PSP_CTRL_CROSS){ side=0; page=0; }
        oldButtons=b; return;
    }
    if(page==0){
        if(pressed&PSP_CTRL_RTRIGGER){ searchActive=1; openOsk(); searchActive=0; }
        if(pressed&PSP_CTRL_LEFT){card=(card+3)%4;}
        if(pressed&PSP_CTRL_RIGHT){card=(card+1)%4;}
        if(pressed&PSP_CTRL_UP){card=(card+2)%4;}
        if(pressed&PSP_CTRL_DOWN){card=(card+1)%4;}
        if(pressed&PSP_CTRL_CROSS)page=1;
    } else {
        if(pressed&PSP_CTRL_CIRCLE)page=0;
    }
    oldButtons=b;
}

static void guInit(){
    sceGuInit();
    void *fb0=guGetStaticVramBuffer(BW,H,GU_PSM_8888), *fb1=guGetStaticVramBuffer(BW,H,GU_PSM_8888), *zb=guGetStaticVramBuffer(BW,H,GU_PSM_4444);
    sceGuStart(GU_DIRECT,list); sceGuDrawBuffer(GU_PSM_8888,fb0,BW); sceGuDispBuffer(W,H,fb1,BW); sceGuDepthBuffer(zb,BW);
    sceGuOffset(2048-W/2,2048-H/2); sceGuViewport(2048,2048,W,H); sceGuScissor(0,0,W,H); sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_TEXTURE_2D); sceGuShadeModel(GU_SMOOTH); sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuDisplay(1);
}
static void frame(){
    sceGuStart(GU_DIRECT,list); sceGuClearColor(BG); sceGuClear(GU_COLOR_BUFFER_BIT);
    if(page==0)home(); else videoPage();
    sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}

int main(){
    callbacks(); sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL); guInit();
    font=intraFontLoad("flash0:/font/ltn0.pgf",INTRAFONT_CACHE_ALL);
    while(running){ input(); frame(); }
    if(font)intraFontUnload(font); sceGuTerm(); return 0;
}
