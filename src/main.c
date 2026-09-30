#include "sif.h"
#include "java_platform.h"
#include "utils/utils.h"
#include <falso_jni/FalsoJNI.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <vitaGL.h>
#ifdef SIF_STALL_DIAGNOSTICS
#include <pthread.h>
static unsigned completed_frames;
static unsigned diagnostic_stop_requested;
static void *frame_watchdog(void *unused) {
    unsigned previous=0,stalled=0;
    for(;;) {
        sceKernelDelayThread(1000000);
        unsigned current=__atomic_load_n(&completed_frames,__ATOMIC_RELAXED);
        stalled=current==previous?stalled+1:0;previous=current;
#ifdef SIF_PRIVATE_DIAGNOSTICS
        if(stalled && stalled%5==0) {
            extern void sif_diagnostic_stall(unsigned,unsigned);
            sif_diagnostic_stall(current,stalled);
        }
#endif
        // Database/asset loading can legitimately block a frame for longer
        // than 30 seconds. Elapsed frame time alone is not a crash condition.
        // Preserve diagnostic cores for explicit compatibility failures.
        if(stalled>=30 && __atomic_load_n(&diagnostic_stop_requested,__ATOMIC_RELAXED)) {
            sif_log("DIAGNOSTIC: explicit STOP at frame %u; intentional watchdog core",current);
            __builtin_trap();
        }
    }
    return NULL;
}
#endif
#ifdef SIF_PRIVATE_DIAGNOSTICS
void sif_diagnostic_startup(void);
void sif_diagnostic_frame(void);
#endif
// Initial bounded heap; revise using measured game allocations.
int _newlib_heap_size_user = 128 * 1024 * 1024;
unsigned int _sceDefaultStackSize = 1024 * 1024;
so_module so_mod;
static SceUID log_fd = -1;
void vita_pcm_rate_trace(uint32_t input_millihz,uint32_t output_hz,uint32_t latency) {
    static unsigned calls;
    if(calls++<16)sif_log("Audio resampler input=%u mHz output=%u Hz filter-latency=%u frames",
        input_millihz,output_hz,latency);
}
void sif_log(const char *fmt,...) {
    char b[2048]; va_list a; va_start(a,fmt);
    int n=vsnprintf(b,sizeof(b)-2,fmt,a); va_end(a);
    if(n<0)return; if(n>(int)sizeof(b)-3)n=sizeof(b)-3;
    b[n++]='\n'; b[n]=0; sceClibPrintf("[SIF] %s",b);
    if(log_fd>=0) sceIoWrite(log_fd,b,n);
}
void sif_stop(const char *reason) {
    sif_log("STOP: %s",reason);
#ifdef SIF_STALL_DIAGNOSTICS
    __atomic_store_n(&diagnostic_stop_requested,1,__ATOMIC_RELAXED);
#endif
    for(;;)sceKernelDelayThread(1000000);
}
static void verify_game(void) {
    unsigned char h[32]; char hex[65]; unsigned char buf[32768];
    SHA256_CTX ctx; SHA256_Init(&ctx);
    FILE *f=fopen(SO_PATH,"rb"); if(!f)sif_stop("libGame.so missing");
    size_t n; while((n=fread(buf,1,sizeof(buf),f)))SHA256_Update(&ctx,buf,n);
    if(ferror(f))sif_stop("libGame.so read failed"); fclose(f);
    SHA256_Final(h,&ctx); for(int i=0;i<32;i++)sprintf(hex+2*i,"%02x",h[i]);
    sif_log("libGame.so SHA256 %s",hex);
    if(strcmp(hex,"8f6df2ab79bbf1065fd26cd2e6fd4172c26f1763460ce4ddef9814412e4460ae"))
        sif_stop("Unsupported game binary");
}
static uintptr_t entry(const char *name) {
    char b[160]; snprintf(b,sizeof(b),"app_klb_android_GameEngine_PFInterface_%s",name);
    uintptr_t p=so_symbol(&so_mod,b); if(!p)sif_stop(b); return p;
}
int main(void) {
    sceIoMkdir(DATA_PATH,0777);
    log_fd=sceIoOpen(DATA_PATH "loader.log",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    sif_log("SIF Vita build " __DATE__ " " __TIME__);
    if(!module_loaded("kubridge"))sif_stop("kubridge not loaded");
    if(!file_exists("ur0:/data/libshacccg.suprx") && !file_exists("ur0:/data/external/libshacccg.suprx"))
        sif_stop("ur0:data/libshacccg.suprx missing");
    verify_game();
    sif_prepare_external_path();
    // A fixed 6 MiB mapping outside the initial host heap; kernel rejects overlap.
    int r=so_file_load(&so_mod,SO_PATH,0x98000000);
    sif_log("so_file_load=%d text=%08x size=%u data=%08x size=%u",r,(unsigned)so_mod.text_base,(unsigned)so_mod.text_size,(unsigned)so_mod.data_base[0],(unsigned)so_mod.data_size[0]);
    if(r<0)sif_stop("so load failed");
    so_relocate(&so_mod);
    if(resolve_imports(&so_mod)<0)sif_stop("Import audit failed");
    so_flush_caches(&so_mod);
    sif_java_init();
#ifdef SIF_PRIVATE_DIAGNOSTICS
    sif_diagnostic_startup();
#endif
    sif_log("constructors: %u",so_mod.num_init_array); so_initialize(&so_mod);
    sif_log("constructors complete");
    ((void(*)(JavaVM*,void*))entry("jniOnLoad"))(&jvm,NULL);
    sif_log("jniOnLoad complete");
    vglSetSemanticBindingMode(VGL_MODE_POSTPONED);
    int fallback=vglInitExtended(0,960,544,4*1024*1024,SCE_GXM_MULTISAMPLE_NONE);
    sif_log("VitaGL fallback=%d version=%s",fallback,glGetString(GL_VERSION));
    sif_log("graphics initialized");
    sif_install_assets();
    typedef jboolean(*Init)(JNIEnv*,jobject,jint,jint,jstring,jstring,jstring,jstring,jstring,jstring);
    jstring p=jni->NewStringUTF(&jni,DATA_PATH);
    jstring model=jni->NewStringUTF(&jni,SIF_DEVICE_MODEL);
    jstring brand=jni->NewStringUTF(&jni,SIF_DEVICE_BRAND);
    jstring board=jni->NewStringUTF(&jni,SIF_DEVICE_BOARD);
    jstring ver=jni->NewStringUTF(&jni,SIF_ANDROID_RELEASE);
    jstring tz=jni->NewStringUTF(&jni,"JST");
    sif_log("initSequence enter");
    jboolean ok=((Init)entry("initSequence"))(&jni,NULL,960,544,p,model,brand,board,ver,tz);
    sif_log("initSequence returned %u",ok);
    if(!ok)sif_stop("game initialization failed");
    sif_input_init();
#ifdef SIF_STALL_DIAGNOSTICS
    pthread_t watchdog;
    if(pthread_create(&watchdog,NULL,frame_watchdog,NULL))sif_stop("Frame watchdog creation failed");
    pthread_detach(watchdog);
#endif
    void(*frame)(JNIEnv*,jobject,jint)=(void*)entry("frameFlip");
    uint64_t last=sceKernelGetProcessTimeWide(),residual=0; unsigned frames=0;
    for(;;) {
        uint64_t now=sceKernelGetProcessTimeWide(); residual+=now-last; last=now;
        unsigned delta=residual/1000; residual%=1000;
        sif_java_platform_frame();
        sif_input_poll();
        sif_java_power_tick();
#ifdef SIF_PRIVATE_DIAGNOSTICS
        sif_diagnostic_frame();
#endif
        frame(&jni,NULL,delta); sif_java_platform_draw();
        vglSwapBuffers(sif_java_platform_modal()?GL_TRUE:GL_FALSE);
#ifdef SIF_STALL_DIAGNOSTICS
        __atomic_add_fetch(&completed_frames,1,__ATOMIC_RELAXED);
#endif
        if(++frames%300==0)sif_log("frames=%u delta=%u",frames,delta);
    }
}
