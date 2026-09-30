#include "stdio_bridge.h"
#include "compat.h"
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
// Leave r/w buffers empty so Android-side inline access falls through.
struct sif_android_file sif_android_stdio[3]={
    {.flags=4,.file=0}, {.flags=8,.file=1}, {.flags=8|2,.file=2}
};
FILE *stdio_host(FILE *f) {
    if((void*)f==&sif_android_stdio[0])return stdin;
    if((void*)f==&sif_android_stdio[1])return stdout;
    if((void*)f==&sif_android_stdio[2])return stderr;
    return f;
}
/* Only for a newly opened, unpublished, read-only regular asset stream.
 * Vita newlib probes metadata when allocating its default buffer and again
 * when first optimizing a seek. Files under the asset mounts are already
 * known regular files. Avoid repeated seek probes, retaining a real fd and
 * newlib's buffer ownership, EOF, ungetc and relative-seek semantics. */
void sif_prepare_read_file(FILE *f) {
#ifdef __NEWLIB__
    int result=setvbuf(f,NULL,_IOFBF,16384);
    if(result==0 && f->_bf._base) {
        f->_blksize=f->_bf._size;
        f->_flags=(f->_flags & ~__SNPT)|__SOPT;
    }
#else
    (void)f;
#endif
}
int sif_fprintf(FILE *f,const char *fmt,...) {va_list a;va_start(a,fmt);int r=vfprintf(stdio_host(f),fmt,a);va_end(a);return r;}
int sif_fscanf(FILE *f,const char *fmt,...) {va_list a;va_start(a,fmt);int r=vfscanf(stdio_host(f),fmt,a);va_end(a);return r;}
int sif_fgetpos(FILE *f,int32_t *pos) {long n=ftell(stdio_host(f));if(n<0)return -1;if(n>INT32_MAX){errno=EOVERFLOW;return -1;}*pos=n;return 0;}
int sif_fsetpos(FILE *f,const int32_t *pos) {return sif_fseek(f,*pos,SEEK_SET);}
int sif_fseeko(FILE *f,int32_t off,int whence) {return sif_fseek(f,off,whence);}
int32_t sif_ftello(FILE *f) {long n=ftell(stdio_host(f));if(n>INT32_MAX){errno=EOVERFLOW;return -1;}return n;}
int sif_feof(FILE *f) {return feof(stdio_host(f));}
int sif_fflush(FILE *f) {return fflush(stdio_host(f));}
int sif_fgetc(FILE *f) {return fgetc(stdio_host(f));}
int sif_getc(FILE *f) {return getc(stdio_host(f));}
char * sif_fgets(char *s,int n,FILE *f) {return fgets(s,n,stdio_host(f));}
int sif_fputc(int c,FILE *f) {return fputc(c,stdio_host(f));}
int sif_putc(int c,FILE *f) {return putc(c,stdio_host(f));}
int sif_fputs(const char *s,FILE *f) {return fputs(s,stdio_host(f));}
size_t sif_fread(void *p,size_t s,size_t n,FILE *f) {return fread(p,s,n,stdio_host(f));}
size_t sif_fwrite(const void *p,size_t s,size_t n,FILE *f) {return fwrite(p,s,n,stdio_host(f));}
FILE * sif_freopen(const char *p,const char *mode,FILE *f) {return freopen(sif_io_path(p),mode,stdio_host(f));}
int sif_fseek(FILE *f,long off,int whence) {
    f=stdio_host(f);
#ifdef SIF_PRIVATE_DIAGNOSTICS
    extern void sif_diagnostic_seek(FILE*,long,int,int);
    sif_diagnostic_seek(f,off,whence,0);
#endif
    /* Newlib's buffered SEEK_END optimization issues fstat on every call.
     * On Vita that metadata query dominates repeated database size checks.
     * Select newlib's normal seek path instead: it still flushes/discards
     * buffers, clears EOF/ungetc state and asks the filesystem for the live
     * end position. No size is cached and no underlying fd is moved behind
     * the FILE object's back. Other seek origins retain their optimization.
     */
#ifdef __NEWLIB__
    int prior_no_opt=f->_flags & __SNPT;
    if(whence==SEEK_END)f->_flags |= __SNPT;
#endif
    int result=fseek(f,off,whence);
#ifdef __NEWLIB__
    if(whence==SEEK_END)f->_flags=(f->_flags & ~__SNPT)|prior_no_opt;
#endif
#ifdef SIF_PRIVATE_DIAGNOSTICS
    sif_diagnostic_seek(f,off,whence,1);
#endif
    return result;
}
long sif_ftell(FILE *f) {return ftell(stdio_host(f));}
int sif_ungetc(int c,FILE *f) {return ungetc(c,stdio_host(f));}
int sif_setvbuf(FILE *f,char *b,int mode,size_t size) {
#ifdef SIF_PRIVATE_DIAGNOSTICS
    extern void sif_diagnostic_buffer(FILE*,int,size_t);
    sif_diagnostic_buffer(stdio_host(f),mode,size);
#endif
    return setvbuf(stdio_host(f),b,mode,size);
}
