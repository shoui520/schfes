#include "sif.h"
#include "npps_server.h"
#include "reimpl/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Points the game at the in-process server. The file is rewritten only when
 * its content differs, so an asset pack copy is shadowed at most once. */
void sif_write_server_info(void) {
    unsigned char *want; size_t size;
    if(sif_server_info_file(&want,&size))sif_stop("server_info.json generation failed");
    const char *path=EXTERNAL_PATH "config/server_info.json";
    int same=0;
    FILE *f=fopen_soloader(path,"rb");
    if(f) {
        unsigned char *have=malloc(size+1);
        if(have) {
            size_t n=fread(have,1,size+1,f);
            same=n==size && !memcmp(have,want,size);
            free(have);
        }
        fclose(f);
    }
    if(!same) {
        f=fopen_soloader(path,"wb");
        if(!f || fwrite(want,1,size,f)!=size || fclose(f))sif_stop("server_info.json write failed");
        sif_log("server_info.json now points to 127.0.0.1:%d",SIF_SERVER_PORT);
    }
    free(want);
}
