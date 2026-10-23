#include "npps_server.h"
#include <psp2/kernel/threadmgr.h>

/* Linked with --wrap=sceKernelCreateThread: threads that leave the affinity
 * to the default (the game, audio, pthreads) stay off the server's core. */
SceUID __real_sceKernelCreateThread(const char *name,SceKernelThreadEntry entry,int priority,SceSize stack,
    SceUInt attr,int affinity,const SceKernelThreadOptParam *option);
SceUID __wrap_sceKernelCreateThread(const char *name,SceKernelThreadEntry entry,int priority,SceSize stack,
    SceUInt attr,int affinity,const SceKernelThreadOptParam *option) {
    if(affinity==SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT || affinity==SCE_KERNEL_CPU_MASK_USER_ALL)
        affinity=SIF_GAME_CPU_MASK;
    return __real_sceKernelCreateThread(name,entry,priority,stack,attr,affinity,option);
}
