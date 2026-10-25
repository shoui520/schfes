#include "asset_pack.h"
#include "sif.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

static const char *const root_names[2]={"install","external"};
static SceUID pack=-1;
static struct sif_pack_header header;
static struct sif_pack_entry *entries;
/* Shadowed path hashes: open addressing, 0 marks an empty slot. Writers hold
 * shadow_lock; readers see either the old or the new table pointer. */
static uint64_t *volatile shadow;
static volatile unsigned shadow_capacity,shadow_count;
static SceKernelLwMutexWork shadow_lock;
static SceUID shadow_log=-1;

#ifdef SIF_PRIVATE_DIAGNOSTICS
#include <psp2/kernel/processmgr.h>
static unsigned diag_reads,diag_hits,diag_misses;static uint64_t diag_bytes,diag_us,diag_max,diag_requested;
static int pack_pread(void *buf,SceSize n,SceOff at) {
    uint64_t t=sceKernelGetProcessTimeWide();int r=sceIoPread(pack,buf,n,at);t=sceKernelGetProcessTimeWide()-t;
    __atomic_add_fetch(&diag_reads,1,__ATOMIC_RELAXED);if(r>0)__atomic_add_fetch(&diag_bytes,r,__ATOMIC_RELAXED);
    __atomic_add_fetch(&diag_us,t,__ATOMIC_RELAXED);if(t>diag_max)diag_max=t;
    return r;
}
void sif_pack_diag_log(void) {
    unsigned n=__atomic_exchange_n(&diag_reads,0,__ATOMIC_RELAXED);
    uint64_t b=__atomic_exchange_n(&diag_bytes,0,__ATOMIC_RELAXED),us=__atomic_exchange_n(&diag_us,0,__ATOMIC_RELAXED),m=diag_max;diag_max=0;
    unsigned hits=__atomic_exchange_n(&diag_hits,0,__ATOMIC_RELAXED),misses=__atomic_exchange_n(&diag_misses,0,__ATOMIC_RELAXED);
    uint64_t req=__atomic_exchange_n(&diag_requested,0,__ATOMIC_RELAXED);
    if(n || hits)sif_log("PACK IO reads=%u kb=%llu wait-ms=%llu max-ms=%llu requested-kb=%llu hits=%u misses=%u",n,(unsigned long long)(b/1024),
        (unsigned long long)(us/1000),(unsigned long long)(m/1000),(unsigned long long)(req/1024),hits,misses);
}
#else
#define pack_pread(buf,n,at) sceIoPread(pack,buf,n,at)
#endif

#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL
static uint64_t fnv(uint64_t h,const char *s,size_t n) {
    for(size_t i=0;i<n;i++) {
        unsigned char c=s[i];if(c>='A' && c<='Z')c+=32;
        h=(h^c)*FNV_PRIME;
    }
    return h;
}
/* Hash of "<root>/<rel>" (case-folded like exFAT), plus every ancestor
 * prefix below and including the root for shadow checks. */
static uint64_t ref_hash(const struct sif_asset_ref *ref,uint64_t *prefixes,unsigned *count) {
    const char *name=root_names[ref->root];
    uint64_t h=fnv(FNV_OFFSET,name,strlen(name));unsigned n=0;
    if(prefixes)prefixes[n++]=h;
    for(const char *p=ref->rel;*p;) {
        const char *end=strchr(p,'/');size_t len=end?(size_t)(end-p):strlen(p);
        h=fnv(fnv(h,"/",1),p,len);
        if(prefixes && n<64)prefixes[n++]=h;
        p+=len;if(*p)p++;
    }
    if(count)*count=n;
    return h;
}
static const struct sif_pack_entry *find(uint64_t hash) {
    unsigned lo=0,hi=header.entry_count;
    while(lo<hi) {
        unsigned mid=(lo+hi)/2;uint64_t h=entries[mid].hash;
        if(h==hash)return &entries[mid];
        if(h<hash)lo=mid+1;else hi=mid;
    }
    return NULL;
}
static int shadow_has(uint64_t h) {
    uint64_t *table=shadow;unsigned cap=shadow_capacity;
    if(!table)return 0;
    for(unsigned i=(unsigned)h&(cap-1);table[i];i=(i+1)&(cap-1))if(table[i]==h)return 1;
    return 0;
}
static int shadowed(const uint64_t *prefixes,unsigned count) {
    if(!shadow_count)return 0;
    for(unsigned i=0;i<count;i++)if(shadow_has(prefixes[i]))return 1;
    return 0;
}
static void shadow_insert_locked(uint64_t h) {
    if(!h)h=1;
    if(shadow_has(h))return;
    if(!shadow || (shadow_count+1)*2>shadow_capacity) {
        unsigned cap=shadow_capacity?shadow_capacity*2:256;
        uint64_t *table=calloc(cap,sizeof(*table));if(!table)sif_stop("Pack shadow allocation failed");
        for(unsigned i=0;i<shadow_capacity;i++) if(shadow[i]) {
            unsigned j=(unsigned)shadow[i]&(cap-1);while(table[j])j=(j+1)&(cap-1);table[j]=shadow[i];
        }
        /* Old tables are leaked deliberately: lock-free readers may hold them. */
        __atomic_store_n(&shadow_capacity,cap,__ATOMIC_RELEASE);
        __atomic_store_n(&shadow,table,__ATOMIC_RELEASE);
    }
    unsigned j=(unsigned)h&(shadow_capacity-1);while(shadow[j])j=(j+1)&(shadow_capacity-1);
    __atomic_store_n(&shadow[j],h,__ATOMIC_RELEASE);
    __atomic_add_fetch(&shadow_count,1,__ATOMIC_RELEASE);
}
static uint64_t text_hash(const char *line,size_t n) {
    struct sif_asset_ref ref;
    size_t root=strcspn(line,"/");if(root>n)root=n;
    ref.root=SIF_ASSET_NONE;
    for(int i=0;i<2;i++)if(root==strlen(root_names[i]) && !strncmp(line,root_names[i],root))ref.root=i;
    if(ref.root<0 || n-root>=sizeof(ref.rel))return 0;
    size_t rel=root<n?n-root-1:0;memcpy(ref.rel,line+n-rel,rel);ref.rel[rel]=0;
    return ref_hash(&ref,NULL,NULL);
}
static void load_shadow(void) {
    SceUID f=sceIoOpen(SIF_PACK_SHADOW_PATH,SCE_O_RDONLY,0);
    if(f>=0) {
        SceOff size=sceIoLseek(f,0,SCE_SEEK_END);char *text=size>0?malloc(size+1):NULL;
        if(text && sceIoPread(f,text,size,0)==size) {
            text[size]=0;
            for(char *line=text;*line;) {
                char *end=strchr(line,'\n');size_t n=end?(size_t)(end-line):strlen(line);
                uint64_t h=text_hash(line,n);if(h)shadow_insert_locked(h);
                line+=n;if(*line)line++;
            }
        }
        free(text);sceIoClose(f);
    }
    shadow_log=sceIoOpen(SIF_PACK_SHADOW_PATH,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0666);
    if(shadow_log<0)sif_stop("Pack shadow journal unavailable");
    sif_log("Pack shadow entries: %u",shadow_count);
}
static void add_shadow(const struct sif_asset_ref *ref,uint64_t hash) {
    sceKernelLockLwMutex(&shadow_lock,1,NULL);
    if(!shadow_has(hash?hash:1)) {
        char line[1100];int n=snprintf(line,sizeof(line),"%s%s%s\n",root_names[ref->root],*ref->rel?"/":"",ref->rel);
        /* Journal first: a crash may hide a pack file but never resurrects one. */
        if(n<0 || n>=(int)sizeof(line) || sceIoWrite(shadow_log,line,n)!=n)sif_stop("Pack shadow journal write failed");
        shadow_insert_locked(hash);
    }
    sceKernelUnlockLwMutex(&shadow_lock,1);
}
static void cache_init(void);
void sif_pack_init(void) {
    sceKernelCreateLwMutex(&shadow_lock,"sif_pack_shadow",0,0,NULL);
    pack=sceIoOpen(SIF_PACK_PATH,SCE_O_RDONLY,0);
    if(pack<0){pack=-1;sif_log("Pack: none at " SIF_PACK_PATH);return;}
    SceOff size=sceIoLseek(pack,0,SCE_SEEK_END);
    if(sceIoPread(pack,&header,sizeof(header),0)!=sizeof(header) || memcmp(header.magic,SIF_PACK_MAGIC,8) ||
       header.version!=SIF_PACK_VERSION || header.header_size!=sizeof(header))
        sif_stop("assets.bin has an unsupported header");
    /* The file is copied by hand; refuse a truncated transfer explicitly. */
    if((uint64_t)size!=header.file_size){sif_log("Pack size %lld expected %llu",(long long)size,(unsigned long long)header.file_size);sif_stop("assets.bin is incomplete");}
    size_t bytes=(size_t)header.entry_count*sizeof(*entries);
    entries=malloc(bytes?bytes:1);
    if(!entries)sif_stop("Pack index allocation failed");
    if(header.index_offset+bytes>header.file_size || sceIoPread(pack,entries,bytes,header.index_offset)!=(int)bytes)
        sif_stop("Pack index read failed");
    if(crc32(0,(const Bytef*)entries,bytes)!=header.index_crc)sif_stop("assets.bin index is corrupt");
    load_shadow();
    cache_init();
    sif_log("Pack: %u entries, %llu bytes, install=%d external=%d",header.entry_count,
        (unsigned long long)header.file_size,sif_pack_has_root(SIF_ASSET_INSTALL),sif_pack_has_root(SIF_ASSET_EXTERNAL));
}
int sif_pack_available(void) { return pack>=0; }
static const struct sif_pack_entry *lookup(const struct sif_asset_ref *ref,uint64_t *hash) {
    if(pack<0 || ref->root<0)return NULL;
    uint64_t prefixes[64];unsigned count;
    uint64_t h=ref_hash(ref,prefixes,&count);if(hash)*hash=h;
    const struct sif_pack_entry *e=find(h);
    return e && !shadowed(prefixes,count)?e:NULL;
}
int sif_pack_has_root(int root) {
    struct sif_asset_ref ref={.root=root};
    const struct sif_pack_entry *e=lookup(&ref,NULL);
    return e && (e->size_flags&SIF_PACK_DIRECTORY);
}
int sif_pack_stat(const struct sif_asset_ref *ref,struct sif_pack_stat *out) {
    const struct sif_pack_entry *e=lookup(ref,NULL);if(!e)return 0;
    out->directory=!!(e->size_flags&SIF_PACK_DIRECTORY);
    out->size=out->directory?0:e->size_flags;out->mtime=header.mtime;
    return 1;
}

/* Virtual descriptors are real descriptors of assets.bin, so their numbers
 * stay unique; the data is read positionally through the shared handle. */
#define MAX_FDS 1024
struct vfd { volatile int active; uint64_t base; uint32_t size,pos; };
static struct vfd fds[MAX_FDS];
static int open_vfd(const struct sif_pack_entry *e) {
    int fd=open(SIF_PACK_PATH,O_RDONLY);
    if(fd<0)return -1;
    if(fd>=MAX_FDS){close(fd);errno=EMFILE;return -1;}
    fds[fd].base=(uint64_t)e->offset16*16;fds[fd].size=e->size_flags;fds[fd].pos=0;
    __atomic_store_n(&fds[fd].active,1,__ATOMIC_RELEASE);
    return fd;
}
int sif_pack_is_fd(int fd) { return fd>=0 && fd<MAX_FDS && __atomic_load_n(&fds[fd].active,__ATOMIC_ACQUIRE); }

/* All pack streams share one cache of BLOCK-aligned pack blocks. The game
 * reopens and rereads the same assets constantly, and every memory card
 * request is expensive no matter how small; aligned blocks also keep each
 * miss on the driver's direct-transfer path. The lock covers lookups and
 * copies only, never the card read itself. */
#define BLOCK 131072
#define CACHE_BLOCKS 192
struct slot { uint64_t key; uint32_t len,used; int loading; };
static struct slot slots[CACHE_BLOCKS];
static unsigned char *arena;
static unsigned slot_count,tick;
static SceKernelLwMutexWork cache_lock;
static void cache_init(void) {
    sceKernelCreateLwMutex(&cache_lock,"sif_pack_cache",0,0,NULL);
    for(slot_count=CACHE_BLOCKS;slot_count && !(arena=malloc((size_t)slot_count*BLOCK));slot_count/=2);
    sif_log("Pack cache: %u KB",slot_count*(BLOCK/1024));
#ifdef SIF_PRIVATE_DIAGNOSTICS
    extern void sif_diag_heap(const char*);sif_diag_heap("after pack cache");
#endif
}
/* Copies from block `key` if cached; returns bytes copied or -1 on a miss. */
static long cache_copy_locked(uint64_t key,uint32_t off,unsigned char *out,size_t want) {
    for(unsigned i=0;i<slot_count;i++) {
        struct slot *s=&slots[i];
        if(s->key!=key+1 || s->loading)continue;
        if(off>=s->len)return 0;
        size_t k=s->len-off<want?s->len-off:want;
        memcpy(out,arena+(size_t)i*BLOCK+off,k);s->used=++tick;
        return k;
    }
    return -1;
}
static ssize_t read_at(uint64_t base,uint32_t size,uint32_t pos,void *buf,size_t n) {
    if(pos>=size)return 0;
    if(n>size-pos)n=size-pos;
    unsigned char *out=buf;size_t done=0;
    while(done<n) {
        uint64_t at=base+pos+done,key=at&~(uint64_t)(BLOCK-1);uint32_t off=at-key;size_t want=n-done;
        /* Bulk aligned reads bypass the cache instead of flushing it. */
        if(!off && want>=2*BLOCK) {
            int r=pack_pread(out+done,want&~(size_t)(BLOCK-1),at);
            if(r<=0){if(r<0 && !done){errno=EIO;return -1;}break;}
            done+=r;continue;
        }
        sceKernelLockLwMutex(&cache_lock,1,NULL);
        long k=cache_copy_locked(key,off,out+done,want);
        if(k>=0) {
            sceKernelUnlockLwMutex(&cache_lock,1);
#ifdef SIF_PRIVATE_DIAGNOSTICS
            __atomic_add_fetch(&diag_hits,1,__ATOMIC_RELAXED);
#endif
            if(!k)break;
            done+=k;continue;
        }
        struct slot *victim=NULL;
        for(unsigned i=0;i<slot_count;i++)
            if(!slots[i].loading && (!victim || slots[i].used<victim->used))victim=&slots[i];
        uint64_t left=header.file_size-key;uint32_t len=left<BLOCK?left:BLOCK;
        if(!victim) {
            /* No cache, or every block is mid-load: read just this span. */
            sceKernelUnlockLwMutex(&cache_lock,1);
            size_t k2=BLOCK-off<want?BLOCK-off:want;
            int r=pack_pread(out+done,k2,at);
            if(r<=0){if(r<0 && !done){errno=EIO;return -1;}break;}
            done+=r;continue;
        }
        victim->key=key+1;victim->loading=1;
        sceKernelUnlockLwMutex(&cache_lock,1);
        unsigned char *data=arena+(size_t)(victim-slots)*BLOCK;
        int r=pack_pread(data,len,key);
#ifdef SIF_PRIVATE_DIAGNOSTICS
        __atomic_add_fetch(&diag_misses,1,__ATOMIC_RELAXED);
#endif
        sceKernelLockLwMutex(&cache_lock,1,NULL);
        victim->loading=0;victim->len=r>0?r:0;victim->used=++tick;
        if(r<=0)victim->key=0;
        k=r>(int)off?cache_copy_locked(key,off,out+done,want):0;
        sceKernelUnlockLwMutex(&cache_lock,1);
        if(k<=0){if(r<0 && !done){errno=EIO;return -1;}break;}
        done+=k;
    }
#ifdef SIF_PRIVATE_DIAGNOSTICS
    __atomic_add_fetch(&diag_requested,done,__ATOMIC_RELAXED);
#endif
    return done;
}
ssize_t sif_pack_read(int fd,void *buf,size_t n) {
    struct vfd *v=&fds[fd];ssize_t r=read_at(v->base,v->size,v->pos,buf,n);
    if(r>0)v->pos+=r;
    return r;
}
static off_t seek_to(uint32_t size,uint32_t pos,off_t offset,int whence) {
    long long target=whence==SEEK_SET?offset:whence==SEEK_CUR?(long long)pos+offset:whence==SEEK_END?(long long)size+offset:-1;
    if(whence<0 || whence>2 || target<0){errno=EINVAL;return -1;}
    if(target>0x7fffffff){errno=EOVERFLOW;return -1;}
    return (off_t)target;
}
off_t sif_pack_lseek(int fd,off_t offset,int whence) {
    struct vfd *v=&fds[fd];off_t r=seek_to(v->size,v->pos,offset,whence);
    if(r>=0)v->pos=r;
    return r;
}
int sif_pack_fstat(int fd,uint32_t *size,uint32_t *mtime) { *size=fds[fd].size;*mtime=header.mtime;return 0; }
void sif_pack_close(int fd) {
    __atomic_store_n(&fds[fd].active,0,__ATOMIC_RELEASE);close(fd);
}

struct cookie { uint64_t base; uint32_t size,pos; int fd; };
static int cookie_read(void *c,char *buf,int n) {
    struct cookie *k=c;ssize_t r=read_at(k->base,k->size,k->pos,buf,n);
    if(r>0)k->pos+=r;
    return r;
}
static int cookie_write(void *c,const char *buf,int n) { errno=EBADF;return -1; }
static fpos_t cookie_seek(void *c,fpos_t offset,int whence) {
    struct cookie *k=c;off_t r=seek_to(k->size,k->pos,offset,whence);
    if(r>=0)k->pos=r;
    return r;
}
static int cookie_close(void *c) {
    struct cookie *k=c;if(k->fd>=0)sif_pack_close(k->fd);
    free(k);return 0;
}
static FILE *cookie_file(uint64_t base,uint32_t size,uint32_t pos,int fd) {
    struct cookie *k=malloc(sizeof(*k));if(!k){errno=ENOMEM;return NULL;}
    *k=(struct cookie){base,size,pos,fd};
    FILE *f=funopen(k,cookie_read,cookie_write,cookie_seek,cookie_close);
    if(!f){free(k);return NULL;}
    /* Matches sif_prepare_read_file for loose streams. */
    setvbuf(f,NULL,_IOFBF,16384);
    return f;
}
FILE *sif_pack_fdopen(int fd,const char *mode) {
    if(strchr(mode,'w') || strchr(mode,'a') || strchr(mode,'+')){errno=EINVAL;return NULL;}
    struct vfd *v=&fds[fd];
    return cookie_file(v->base,v->size,v->pos,fd);
}
static int writes(int newlib_flags) {
    return (newlib_flags&O_ACCMODE)!=O_RDONLY || (newlib_flags&(O_CREAT|O_TRUNC|O_APPEND));
}
int sif_pack_open(const struct sif_asset_ref *ref,int flags,int *fd) {
    if(pack<0 || ref->root<0)return 0;
    if(writes(flags)) {
        if(sif_pack_before_write(ref,(flags&O_TRUNC)?SIF_PACK_REPLACE:SIF_PACK_MODIFY)<0){*fd=-1;return 1;}
        return 0;
    }
    const struct sif_pack_entry *e=lookup(ref,NULL);
    if(!e)return 0;
    if(e->size_flags&SIF_PACK_DIRECTORY){*fd=-1;errno=EISDIR;return 1;}
    *fd=open_vfd(e);return 1;
}
int sif_pack_fopen(const struct sif_asset_ref *ref,const char *mode,FILE **file) {
    if(pack<0 || ref->root<0)return 0;
    if(strchr(mode,'w') || strchr(mode,'a') || strchr(mode,'+')) {
        if(sif_pack_before_write(ref,strchr(mode,'w')?SIF_PACK_REPLACE:SIF_PACK_MODIFY)<0){*file=NULL;return 1;}
        return 0;
    }
    const struct sif_pack_entry *e=lookup(ref,NULL);
    if(!e)return 0;
    if(e->size_flags&SIF_PACK_DIRECTORY){*file=NULL;errno=EISDIR;return 1;}
    *file=cookie_file((uint64_t)e->offset16*16,e->size_flags,0,-1);return 1;
}
int sif_pack_make_parents(const char *physical) {
    char path[1100];size_t n=strlen(physical);
    if(n>=sizeof(path)){errno=ENAMETOOLONG;return -1;}
    memcpy(path,physical,n+1);
    for(char *c=path+5;*c;c++) {
        if(*c!='/')continue;
        *c=0;int r=sceIoMkdir(path,0777);*c='/';
        if(r<0 && (unsigned)r!=0x80010011)return -1; /* SCE_ERROR_ERRNO_EEXIST */
    }
    return 0;
}
static int copy_up(const struct sif_pack_entry *e,const char *physical) {
    if(sif_pack_make_parents(physical)<0)return -1;
    char tmp[1110];snprintf(tmp,sizeof(tmp),"%s.siftmp",physical);
    SceUID out=sceIoOpen(tmp,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(out<0){errno=EIO;return -1;}
    static char buf[65536];static SceKernelLwMutexWork lock;static int ready;
    if(!__atomic_exchange_n(&ready,1,__ATOMIC_ACQ_REL))sceKernelCreateLwMutex(&lock,"sif_pack_copy",0,0,NULL);
    sceKernelLockLwMutex(&lock,1,NULL);
    uint64_t base=(uint64_t)e->offset16*16;uint32_t size=e->size_flags,done=0;int ok=1;
    while(ok && done<size) {
        uint32_t n=size-done<sizeof(buf)?size-done:sizeof(buf);
        ok=sceIoPread(pack,buf,n,base+done)==(int)n && sceIoWrite(out,buf,n)==(int)n;
        done+=n;
    }
    sceKernelUnlockLwMutex(&lock,1);
    if(sceIoClose(out)<0)ok=0;
    sceIoRemove(physical);
    if(!ok || sceIoRename(tmp,physical)<0){sceIoRemove(tmp);errno=EIO;return -1;}
    return 0;
}
int sif_pack_before_write(const struct sif_asset_ref *ref,int kind) {
    uint64_t hash;const struct sif_pack_entry *e=lookup(ref,&hash);
    if(!e)return 0;
    char physical[1100];
    if(sif_asset_physical(ref,physical,sizeof(physical))<0)return -1;
    if(e->size_flags&SIF_PACK_DIRECTORY) {
        if(kind!=SIF_PACK_REMOVE){errno=EISDIR;return -1;}
    } else if(kind==SIF_PACK_MODIFY) {
        if(copy_up(e,physical)<0)return -1;
        sif_log("Pack copy-up %s/%s",root_names[ref->root],ref->rel);
    } else if(kind==SIF_PACK_REPLACE && sif_pack_make_parents(physical)<0) return -1;
    add_shadow(ref,hash);
    return 0;
}

/* Listings are materialized on open; directories hold at most a few
 * thousand entries. */
#define DIR_MAGIC 0x53504452u
struct pack_dir { uint32_t magic; unsigned count,next; struct { unsigned char type; char *name; } *items; };
struct name_set { uint64_t *slots; unsigned capacity,count; };
static int set_add(struct name_set *s,uint64_t h) {
    if(!h)h=1;
    if((s->count+1)*2>s->capacity) {
        struct name_set grown={calloc(s->capacity*2,sizeof(uint64_t)),s->capacity*2,0};
        if(!grown.slots)sif_stop("Pack listing allocation failed");
        for(unsigned i=0;i<s->capacity;i++)if(s->slots[i])set_add(&grown,s->slots[i]);
        free(s->slots);*s=grown;
    }
    unsigned i=(unsigned)h&(s->capacity-1);
    for(;s->slots[i];i=(i+1)&(s->capacity-1))if(s->slots[i]==h)return 0;
    s->slots[i]=h;s->count++;return 1;
}
DIR *sif_pack_opendir(const struct sif_asset_ref *ref,const char *physical) {
    const struct sif_pack_entry *e=lookup(ref,NULL);
    if(!e)return NULL;
    if(!(e->size_flags&SIF_PACK_DIRECTORY)){errno=ENOTDIR;return (DIR*)-1;}
    uint32_t bytes=e->size_flags&~SIF_PACK_DIRECTORY;
    struct pack_dir *d=calloc(1,sizeof(*d));
    unsigned char *blob=malloc(bytes+1);
    if(!d || !blob || sceIoPread(pack,blob,bytes,(uint64_t)e->offset16*16)!=(int)bytes){free(d);free(blob);errno=EIO;return (DIR*)-1;}
    unsigned pack_count=0;for(uint32_t p=0;p+2<=bytes;p+=2+blob[p+1])pack_count++;
    DIR *loose=opendir(physical);
    unsigned capacity=pack_count+64;
    d->items=malloc(capacity*sizeof(*d->items));
    struct name_set seen={NULL,256,0};
    while(seen.capacity<2*capacity)seen.capacity*=2;
    seen.slots=calloc(seen.capacity,sizeof(uint64_t));
    if(!d->items || !seen.slots)sif_stop("Pack listing allocation failed");
    if(loose) {
        for(struct dirent *de;(de=readdir(loose));) {
            if(!strcmp(de->d_name,".") || !strcmp(de->d_name,".."))continue;
            if(!set_add(&seen,fnv(FNV_OFFSET,de->d_name,strlen(de->d_name))))continue;
            if(d->count==capacity){capacity*=2;d->items=realloc(d->items,capacity*sizeof(*d->items));if(!d->items)sif_stop("Pack listing allocation failed");}
            d->items[d->count].type=SCE_S_ISDIR(de->d_stat.st_mode)?DT_DIR:DT_REG;
            d->items[d->count++].name=strdup(de->d_name);
        }
        closedir(loose);
    }
    struct sif_asset_ref child;child.root=ref->root;
    for(uint32_t p=0;p+2<=bytes;) {
        unsigned char type=blob[p],len=blob[p+1];const char *name=(const char*)blob+p+2;p+=2+len;
        if(p>bytes)break;
        int n=snprintf(child.rel,sizeof(child.rel),"%s%s%.*s",ref->rel,*ref->rel?"/":"",(int)len,name);
        if(n<0 || n>=(int)sizeof(child.rel) || !lookup(&child,NULL) || !set_add(&seen,fnv(FNV_OFFSET,name,len)))continue;
        if(d->count==capacity){capacity*=2;d->items=realloc(d->items,capacity*sizeof(*d->items));if(!d->items)sif_stop("Pack listing allocation failed");}
        d->items[d->count].type=type==2?DT_DIR:DT_REG;
        d->items[d->count++].name=strndup(name,len);
    }
    free(seen.slots);free(blob);
    d->magic=DIR_MAGIC;
    return (DIR*)d;
}
int sif_pack_is_dir(DIR *dir) { return dir && ((struct pack_dir*)dir)->magic==DIR_MAGIC; }
const char *sif_pack_readdir(DIR *dir,unsigned char *type) {
    struct pack_dir *d=(struct pack_dir*)dir;
    if(d->next>=d->count)return NULL;
    *type=d->items[d->next].type;return d->items[d->next++].name;
}
void sif_pack_closedir(DIR *dir) {
    struct pack_dir *d=(struct pack_dir*)dir;d->magic=0;
    for(unsigned i=0;i<d->count;i++)free(d->items[i].name);
    free(d->items);free(d);
}
