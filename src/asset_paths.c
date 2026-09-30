#include "asset_paths.h"
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *sif_install_path=INSTALL_ROOT "/";
const char *sif_external_path=EXTERNAL_ROOT "/";
static int select_root(const char *const *roots,unsigned count,const char **selected) {
    for(unsigned i=0;i<count;i++) {
        struct stat st;
        if(stat(roots[i],&st)==0) {
            if(!S_ISDIR(st.st_mode)){errno=ENOTDIR;return -1;}
            *selected=roots[i];return 0;
        }
        if(errno!=ENOENT)return -1;
    }
    if(mkdir(roots[0],0777)<0)return -1;
    *selected=roots[0];return 0;
}
int sif_select_asset_paths(void) {
    static const char *const install[]={INSTALL_ROOT "/",DEV_INSTALL_ROOT "/"};
    static const char *const external[]={EXTERNAL_ROOT "/",DEV_EXTERNAL_ROOT "/",LEGACY_EXTERNAL_ROOT "/"};
    if(select_root(install,2,&sif_install_path)<0)return -1;
    return select_root(external,3,&sif_external_path);
}
const char *sif_resolve_asset_path(const char *path) {
    static const char *const aliases[]={INSTALL_ROOT,DEV_INSTALL_ROOT,EXTERNAL_ROOT,DEV_EXTERNAL_ROOT,LEGACY_EXTERNAL_ROOT};
    static _Thread_local char mapped[4][1024];
    static _Thread_local unsigned next;
    if(!path)return NULL;
    for(unsigned i=0;i<5;i++) {
        const char *root=aliases[i];size_t n=strlen(root);
        /* Vita accepts both ux0:/foo and ux0:foo spellings. */
        int compact=!strncmp(path,"ux0:",4) && path[4]!='/';
        int match=compact ? !strncmp(path+4,root+5,n-5) : !strncmp(path,root,n);
        size_t prefix=n-(compact?1:0);
        if(!match || (path[prefix] && path[prefix]!='/'))continue;
        const char *suffix=path+prefix;if(*suffix=='/')suffix++;
        const char *selected=i<2?sif_install_path:sif_external_path;
        char *out=mapped[next++%4];
        int len=snprintf(out,1024,"%s%s",selected,suffix);
        if(len<0 || len>=1024){errno=ENAMETOOLONG;return NULL;}
        /* Preserve root spelling without trailing slash for rename/stat. */
        if(!path[prefix] && len && out[len-1]=='/')out[len-1]=0;
        return out;
    }
    return path;
}
