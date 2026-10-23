#include "sif.h"
#include "compat.h"
#include "asset_pack.h"
#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <setjmp.h>
#include <malloc.h>
#include <vitaGL.h>
#include <stdio.h>
#include <sys/stat.h>
_Static_assert(sizeof(int32_t)==4,"Android time word");
_Static_assert(sizeof(struct sif_time32)==8,"Android timeval/timespec");
static char dl_error[256];
extern void *dlsym_soloader(void *, const char *);
void sif_assert(const char *file,int line,const char *fun,const char *why) {
    sif_log("Assertion %s:%d %s: %s",file,line,fun,why); sif_stop("Game assertion");
}
extern uint32_t __exidx_start[],__exidx_end[];
uintptr_t sif_unwind(uintptr_t pc,int *count) {
    if(pc>=so_mod.text_base && pc<so_mod.text_base+so_mod.text_size) {
        *count=so_mod.exidx_size/8; return so_mod.exidx_base;
    }
    *count=((uintptr_t)__exidx_end-(uintptr_t)__exidx_start)/8;
    return (uintptr_t)__exidx_start;
}
uintptr_t __gnu_Unwind_Find_exidx(uintptr_t pc,int *count) { return sif_unwind(pc,count); }
void *sif_dlopen(const char *path,int flags) {
    if(!path || strstr(path,"libGLES") || strstr(path,"libOpenSLES") || strstr(path,"libGame") || strstr(path,"libc.so") || strstr(path,"libm.so")) {
        sif_log("dlopen(%s,%d): compatibility namespace",path?path:"self",flags); return &so_mod;
    }
    snprintf(dl_error,sizeof(dl_error),"Unsupported module: %s",path);
    sif_log("dlopen: %s",dl_error);return NULL;
}
int sif_dlclose(void *handle) {
    if(handle==&so_mod)return 0;
    strcpy(dl_error,"Invalid module handle");return -1;
}
char *sif_dlerror(void) {
    static char result[256]; if(!dl_error[0])return NULL;
    strcpy(result,dl_error);dl_error[0]=0;return result;
}
int sif_dladdr(const void *addr,void *out) {
    struct Info { const char *file; void *base; const char *name; void *symbol; } *i=out;
    uintptr_t a=(uintptr_t)addr;
    if(a<so_mod.text_base || a>=so_mod.text_base+so_mod.text_size)return 0;
    *i=(struct Info){ SO_PATH,(void*)so_mod.load_addr,NULL,NULL };
    uintptr_t best=0;
    for(unsigned k=0;k<so_mod.num_dynsym;k++) {
        Elf32_Sym *s=&so_mod.dynsym[k];
        if(s->st_shndx==SHN_UNDEF || !s->st_value)continue;
        uintptr_t p=so_mod.load_addr+s->st_value;
        if(p<=a && p>best) {best=p;i->name=so_mod.dynstr+s->st_name;i->symbol=(void*)p;}
    }
    return 1;
}
int sif_fchown(int fd,unsigned uid,unsigned gid) { sif_log("fchown unsupported fd=%d",fd);errno=ENOSYS;return -1; }
int sif_geteuid(void) { return 1000; }
void *sif_getpwuid(unsigned uid) { errno=ENOENT;return NULL; }
int sif_getrusage(int who,void *out) { sif_log("getrusage unsupported who=%d",who);errno=ENOSYS;return -1; }
int sif_gettid(void) { return sceKernelGetThreadId(); }
unsigned sif_alarm(unsigned seconds) { if(seconds)sif_stop("POSIX alarms unsupported");return 0; }
unsigned sif_if_nametoindex(const char *name) { errno=ENODEV;return 0; }
int sif_statfs(const char *path,void *out) {
    // Android ARM statfs uses 64-bit block counters (Bionic sys/vfs.h).
    struct Statfs { uint32_t type,bsize; uint64_t blocks,bfree,bavail,files,ffree; int32_t fsid[2]; uint32_t namelen,frsize,flags,spare[4]; } *s=out;
    if(strncmp(path,"ux0:",4)) { errno=ENOENT;return -1; }
    uint64_t max,free;int r=sceAppMgrGetDevInfo("ux0:",&max,&free);
    if(r<0) { errno=EIO;return -1; }
    memset(s,0,sizeof(*s));s->type=0x4d44;s->bsize=s->frsize=4096;
    s->blocks=max/4096;s->bfree=s->bavail=free/4096;s->namelen=255;
    return 0;
}
long sif_sysconf(int name) {
    switch(name) {
        case 0x27: return 4096; // Bionic _SC_PAGESIZE.
        default: sif_log("sysconf unsupported id=%d",name);errno=EINVAL;return -1;
    }
}
unsigned sif_file_mask=022;
unsigned sif_umask(unsigned mask) { return __sync_lock_test_and_set(&sif_file_mask,mask&0777); }
static const char *mount_path(const char *path) {
    // Embedded Unix VFS code treats a Vita mount path as relative and prefixes
    // getcwd(). The captured database failure is app0:/ux0:/data/.... Resolve
    // that representation at the host filesystem boundary, keeping other
    // relative paths and native mount paths intact.
    const char *candidate=path;
    if(!strncmp(candidate,"app0:/",6))candidate+=6;
    else if(*candidate=='/')candidate++;
    if(candidate!=path && (!strncmp(candidate,"ux0:",4) || !strncmp(candidate,"app0:/",6))) {
        sif_log("Filesystem mount path: %s -> %s",path,candidate);
        return candidate;
    }
    return path;
}
const char *sif_io_target(const char *path,struct sif_asset_ref *ref) {
    static _Thread_local char mapped[4][1100];
    static _Thread_local unsigned next;
    ref->root=SIF_ASSET_NONE;
    if(!path)return NULL;
    path=mount_path(path);
    if(sif_asset_split(path,ref)==SIF_ASSET_NONE)return path;
    char *out=mapped[next++%4];
    if(sif_asset_physical(ref,out,sizeof(mapped[0]))<0)sif_stop("Asset path exceeds bridge capacity");
    return out;
}
const char *sif_io_path(const char *path) {
    struct sif_asset_ref ref;
    return sif_io_target(path,&ref);
}
int sif_access(const char *path,int mode) {
    struct sif_asset_ref ref;struct sif_pack_stat st;
    path=sif_io_target(path,&ref);
    return sif_pack_stat(&ref,&st)?0:access(path,mode);
}
/* A pack entry that is removed is shadowed; it never existed loose, so a
 * missing loose copy still means success. */
static int remove_path(const char *path,int (*op)(const char*)) {
    struct sif_asset_ref ref;struct sif_pack_stat st;
    path=sif_io_target(path,&ref);
    int packed=sif_pack_stat(&ref,&st);
    if(packed && sif_pack_before_write(&ref,SIF_PACK_REMOVE)<0)return -1;
    int r=op(path);
    if(r<0 && packed && errno==ENOENT)return 0;
    return r;
}
int sif_remove(const char *path) {return remove_path(path,remove);}
int sif_rmdir(const char *path) {return remove_path(path,rmdir);}
int sif_unlink(const char *path) {return remove_path(path,unlink);}
int sif_rename(const char *old,const char *next) {
    struct sif_asset_ref from,to;struct sif_pack_stat st;
    old=sif_io_target(old,&from);next=sif_io_target(next,&to);
    if(sif_pack_stat(&from,&st) && st.directory) {
        sif_log("rename of packed directory %s is unsupported",old);
        errno=EXDEV;return -1;
    }
    if(sif_pack_before_write(&from,SIF_PACK_MODIFY)<0 || sif_pack_before_write(&to,SIF_PACK_REPLACE)<0)return -1;
    int r=rename(old,next);
    if(r<0 && errno==ENOENT && to.root>=0 && sif_pack_make_parents(next)==0)r=rename(old,next);
    return r;
}
int sif_mkdir(const char *path,unsigned mode) {
    struct sif_asset_ref ref;struct sif_pack_stat st;
    path=sif_io_target(path,&ref);
    if(sif_pack_stat(&ref,&st)){errno=EEXIST;return -1;}
    int r=mkdir(path,mode&~sif_file_mask);
    if(r<0 && errno==ENOENT && ref.root>=0 && sif_pack_make_parents(path)==0)r=mkdir(path,mode&~sif_file_mask);
    return r;
}
int sif_utimes(const char *path,const struct sif_time32 *tv) {
    struct sif_asset_ref ref;struct sif_pack_stat st;
    path=sif_io_target(path,&ref);
    if(sif_pack_stat(&ref,&st))return 0;
    if(!tv)return utimes(path,NULL);
    struct timeval t[2]={{tv[0].sec,tv[0].sub},{tv[1].sec,tv[1].sub}};
    return utimes(path,t);
}
int sif_gettimeofday(struct sif_time32 *out,void *tz) {
    struct timeval t;int r=gettimeofday(&t,NULL);
    if(out){out->sec=t.tv_sec;out->sub=t.tv_usec;} return r;
}
int sif_clock_gettime(int id,struct sif_time32 *out) {
    if(!out){errno=EFAULT;return -1;}
    uint64_t us;
    switch(id) {
        case 0: { SceRtcTick t;int r=sceRtcGetCurrentTick(&t);if(r<0){errno=EIO;return -1;}us=t.tick-62135587294000000ULL;break; }
        case 1: case 4: case 6: case 7: us=sceKernelGetProcessTimeWide();break;
        default: sif_log("clock_gettime unsupported id=%d",id);errno=EINVAL;return -1;
    }
    out->sec=us/1000000;out->sub=(us%1000000)*1000;return 0;
}
int32_t sif_time(int32_t *out) { time_t t=time(NULL);if(out)*out=t;return t; }
struct tm *sif_gmtime(const int32_t *t) { time_t v=*t;return gmtime(&v); }
struct tm *sif_gmtime_r(const int32_t *t,struct tm *out) { time_t v=*t;return gmtime_r(&v,out); }
struct tm *sif_localtime(const int32_t *t) { time_t v=*t;return localtime(&v); }
int32_t sif_mktime(struct tm *t) { return mktime(t); }
// Android ARM jmp_buf reserves 256 bytes; newlib's buffer fits. Use a tail
// branch so the saved context belongs to the original caller. Signal masks
// are unavailable and a nonzero savesigs stops before changing the buffer.
void sif_signal_mask_unsupported(void) { sif_stop("sigsetjmp signal masks unsupported"); }
__attribute__((naked,returns_twice)) int sif_sigsetjmp(void *buf,int mask) {
    __asm__ volatile("cmp r1,#0\nbne 1f\nb setjmp\n1: b sif_signal_mask_unsupported\n");
}
__attribute__((naked,noreturn)) void sif_siglongjmp(void *buf,int value) { __asm__ volatile("b longjmp\n"); }
void sif_compressed_sub(unsigned target,int level,int x,int y,int w,int h,unsigned format,int size,const void *data) {
    sif_log("compressed subimage target=%x level=%d xy=%d,%d size=%d,%d format=%x bytes=%d",target,level,x,y,w,h,format,size);
    sif_stop("Compressed texture subimage path needs implementation");
}
