#pragma once
#include <stdio.h>
#include <stdint.h>
/* Android ARM FILE layout: bionic android-4.4_r1 libc/include/stdio.h. */
struct sif_android_file {
    unsigned char *p;int r,w;int16_t flags,file;
    struct {unsigned char *base;int size;} bf;int lbfsize;
    void *cookie;void *close,*read,*seek,*write;
    struct {unsigned char *base;int size;} ext;
    unsigned char *up;int ur;unsigned char ubuf[3],nbuf[1];
    struct {unsigned char *base;int size;} lb;
    int blksize;int32_t offset;
};
_Static_assert(sizeof(struct sif_android_file)==84,"Bionic FILE size");
_Static_assert(__builtin_offsetof(struct sif_android_file,file)==14,"Bionic FILE fd offset");
extern struct sif_android_file sif_android_stdio[3];
FILE *stdio_host(FILE*);
void sif_prepare_read_file(FILE*);
int sif_fprintf(FILE*,const char*,...);
int sif_fscanf(FILE*,const char*,...);
int sif_fgetpos(FILE*,int32_t*);
int sif_fsetpos(FILE*,const int32_t*);
int sif_fseeko(FILE*,int32_t,int);
int32_t sif_ftello(FILE*);
int sif_feof(FILE *f);
int sif_fflush(FILE *f);
int sif_fgetc(FILE *f);
int sif_getc(FILE *f);
char * sif_fgets(char *s,int n,FILE *f);
int sif_fputc(int c,FILE *f);
int sif_putc(int c,FILE *f);
int sif_fputs(const char *s,FILE *f);
size_t sif_fread(void *p,size_t s,size_t n,FILE *f);
size_t sif_fwrite(const void *p,size_t s,size_t n,FILE *f);
FILE * sif_freopen(const char *p,const char *mode,FILE *f);
int sif_fseek(FILE *f,long off,int whence);
long sif_ftell(FILE *f);
int sif_ungetc(int c,FILE *f);
int sif_setvbuf(FILE *f,char *b,int mode,size_t size);
