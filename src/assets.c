#include "sif.h"
#include "utils/utils.h"
#include <minizip/unzip.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <zlib.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
void sif_prepare_external_path(void) {
    if(sif_select_asset_paths()<0)sif_stop("Asset directory selection failed");
    sif_log("Asset roots: install=%s external=%s",INSTALL_PATH,EXTERNAL_PATH);
}
static int valid_existing(const char *path,const unz_file_info *info,unsigned char *buf,size_t capacity) {
    SceIoStat st;
    if(sceIoGetstat(path,&st)<0 || SCE_S_ISDIR(st.st_mode) || st.st_size!=info->uncompressed_size)return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    uLong crc=crc32(0,NULL,0);size_t n;
    while((n=fread(buf,1,capacity,f)))crc=crc32(crc,buf,n);
    int bad=ferror(f);fclose(f);
    return !bad && crc==info->crc;
}
// Match Android's install/external overlay layout. Game data remains supplied
// separately; it is never part of the VPK.
void sif_install_assets(void) {
    if(file_exists(DATA_PATH "assets-v9.11.ready"))return;
    unzFile z=unzOpen(DATA_PATH "AppAssets.zip");
    if(!z)sif_stop("AppAssets.zip missing or invalid");
    unz_global_info gi;if(unzGetGlobalInfo(z,&gi)!=UNZ_OK || gi.number_entry!=8462)
        sif_stop("Unsupported asset archive");
    sceIoMkdir(INSTALL_PATH,0777);sceIoMkdir(EXTERNAL_PATH,0777);
    char name[768],path[1024],tmp[1040]; unsigned char buf[32768];
    unsigned files=0;unsigned long long bytes=0;
    sif_log("Installing assets: %lu entries",gi.number_entry);
    int r=unzGoToFirstFile(z);
    for(unsigned long i=0;i<gi.number_entry;i++) {
        unz_file_info info;
        if(r!=UNZ_OK || unzGetCurrentFileInfo(z,&info,name,sizeof(name),NULL,0,NULL,0)!=UNZ_OK || info.size_filename>=sizeof(name))
            sif_stop("Invalid ZIP entry");
        if(name[0]=='/' || strchr(name,':') || strchr(name,'\\'))sif_stop("Invalid ZIP path");
        char check[768];strcpy(check,name);char *save=NULL;
        for(char *c=strtok_r(check,"/",&save);c;c=strtok_r(NULL,"/",&save))
            if(!strcmp(c,"..") || !strcmp(c,"."))sif_stop("Invalid ZIP path component");
        snprintf(path,sizeof(path),"%s%s",INSTALL_PATH,name);
        if(!file_mkpath(path,0777))sif_stop("Asset directory creation failed");
        size_t len=strlen(name);
        if(len && name[len-1]!='/') {
            if(valid_existing(path,&info,buf,sizeof(buf))) {
                bytes+=info.uncompressed_size;files++;
                goto progress;
            }
            if(unzOpenCurrentFile(z)!=UNZ_OK)sif_stop("ZIP entry open failed");
            snprintf(tmp,sizeof(tmp),"%s.siftmp",path);
            FILE *f=fopen(tmp,"wb");if(!f) {
                int e=errno;uint64_t free_bytes=0,total_bytes=0;
                int dev=sceAppMgrGetDevInfo("ux0:",&total_bytes,&free_bytes);
                SceUID probe=sceIoOpen(tmp,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
                sif_log("Asset create entry=%lu path=%s errno=%d (%s) kernel=%08x storage=%d free=%llu total=%llu",i+1,tmp,e,strerror(e),(unsigned)probe,dev,(unsigned long long)free_bytes,(unsigned long long)total_bytes);
                if(probe>=0)sceIoClose(probe);
                char dir[1024];strcpy(dir,path);char *slash=strrchr(dir,'/');if(slash)*slash=0;
                SceIoStat parent;int pr=sceIoGetstat(dir,&parent);
                sif_log("Asset parent stat=%08x mode=%08x",pr,pr>=0?(unsigned)parent.st_mode:0);
                sif_stop("Asset file creation failed");
            }
            int n;unsigned total=0;
            while((n=unzReadCurrentFile(z,buf,sizeof(buf)))>0) {
                if(fwrite(buf,1,n,f)!=(unsigned)n)sif_stop("Asset write failed");
                total+=n;
            }
            int closed=fclose(f),crc=unzCloseCurrentFile(z);
            if(n<0 || closed || crc!=UNZ_OK || total!=info.uncompressed_size)sif_stop("Asset integrity failed");
            if(rename(tmp,path))sif_stop("Asset finalize failed");
            bytes+=total;files++;
        }
progress:
        if((i+1)%500==0)sif_log("Assets %lu/%lu: %llu bytes",i+1,gi.number_entry,bytes);
        r=unzGoToNextFile(z);
    }
    if(unzClose(z)!=UNZ_OK || bytes!=122822321ULL)sif_stop("Asset archive total mismatch");
    FILE *f=fopen(DATA_PATH "assets-v9.11.ready.tmp","wb");
    if(!f)sif_stop("Asset marker creation failed");
    if(fprintf(f,"9.11 %u %llu\n",files,bytes)<0 || fclose(f))sif_stop("Asset marker write failed");
    if(rename(DATA_PATH "assets-v9.11.ready.tmp",DATA_PATH "assets-v9.11.ready"))sif_stop("Asset marker finalize failed");
    sif_log("Assets installed: %u files %llu bytes",files,bytes);
}
