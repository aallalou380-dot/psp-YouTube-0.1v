#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("PSP YouTube", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback",
                                        exit_callback,
                                        NULL);

    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();

    return 0;
}

int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread("CallbackThread",
                                     CallbackThread,
                                     0x11,
                                     0xFA0,
                                     0,
                                     NULL);

    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);

    return thid;
}

int main(void)
{
    SetupCallbacks();

    pspDebugScreenInit();

    pspDebugScreenSetXY(5, 2);
    pspDebugScreenPrintf("PSP YouTube 0.1");

    pspDebugScreenSetXY(5, 4);
    pspDebugScreenPrintf("----------------------");

    pspDebugScreenSetXY(5, 6);
    pspDebugScreenPrintf("> Search");

    pspDebugScreenSetXY(5, 7);
    pspDebugScreenPrintf("  Trending");

    pspDebugScreenSetXY(5, 8);
    pspDebugScreenPrintf("  Categories");

    pspDebugScreenSetXY(5, 9);
    pspDebugScreenPrintf("  Settings");

    pspDebugScreenSetXY(5, 12);
    pspDebugScreenPrintf("UP/DOWN  Select");

    pspDebugScreenSetXY(5, 13);
    pspDebugScreenPrintf("START    Exit");

    SceCtrlData pad;

    while (1)
    {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();

    return 0;
}
