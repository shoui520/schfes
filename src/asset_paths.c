#include "asset_paths.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char *const roots[2]={INSTALL_ROOT,EXTERNAL_ROOT};
/* data/sif/<root> uses three of the process's path components. */
#define REL_COMPONENTS (SIF_MAX_PATH_COMPONENTS-3)

static int normalize(const char *src,char *dst,size_t capacity) {
    size_t n=0;
    while(*src) {
        while(*src=='/')src++;
        if(!*src)break;
        const char *end=strchr(src,'/');size_t len=end?(size_t)(end-src):strlen(src);
        if(!(len==1 && src[0]=='.')) {
            if(n+len+2>capacity){errno=ENAMETOOLONG;return -1;}
            if(n)dst[n++]='/';
            memcpy(dst+n,src,len);n+=len;
        }
        src+=len;
    }
    dst[n]=0;return 0;
}
int sif_asset_split(const char *path,struct sif_asset_ref *out) {
    out->root=SIF_ASSET_NONE;out->rel[0]=0;
    if(!path || strncmp(path,"ux0:",4))return SIF_ASSET_NONE;
    /* Vita accepts both ux0:/foo and ux0:foo spellings. */
    const char *p=path+4;if(*p=='/')p++;
    for(int i=0;i<2;i++) {
        const char *root=roots[i]+5;size_t n=strlen(root);
        if(strncmp(p,root,n) || (p[n] && p[n]!='/'))continue;
        if(normalize(p+n,out->rel,sizeof(out->rel))<0)return SIF_ASSET_NONE;
        return out->root=i;
    }
    return SIF_ASSET_NONE;
}
int sif_asset_physical(const struct sif_asset_ref *ref,char *out,size_t capacity) {
    const char *rel=ref->rel;unsigned parts=*rel?1:0;
    for(const char *c=rel;*c;c++)parts+=*c=='/';
    int len=snprintf(out,capacity,"%s%s%s",roots[ref->root],*rel?"/":"",rel);
    if(len<0 || (size_t)len>=capacity){errno=ENAMETOOLONG;return -1;}
    if(parts>REL_COMPONENTS) {
        /* Join the leading components so exactly REL_COMPONENTS remain. */
        char *c=out+strlen(roots[ref->root])+1;
        for(unsigned joins=parts-REL_COMPONENTS;joins;c++)
            if(*c=='/'){*c=SIF_COLLAPSE_SEPARATOR;joins--;}
    }
    return 0;
}
const char *sif_asset_uncollapsed(const struct sif_asset_ref *ref) {
    static _Thread_local char plain[1100];
    unsigned parts=*ref->rel?1:0;
    for(const char *c=ref->rel;*c;c++)parts+=*c=='/';
    if(ref->root<0 || parts<=REL_COMPONENTS)return NULL;
    int len=snprintf(plain,sizeof(plain),"%s/%s",roots[ref->root],ref->rel);
    return len<0 || (size_t)len>=sizeof(plain)?NULL:plain;
}
const char *sif_resolve_asset_path(const char *path) {
    static _Thread_local char mapped[4][1100];
    static _Thread_local unsigned next;
    struct sif_asset_ref ref;
    if(sif_asset_split(path,&ref)==SIF_ASSET_NONE)return path;
    char *out=mapped[next++%4];
    return sif_asset_physical(&ref,out,sizeof(mapped[0]))<0?NULL:out;
}
