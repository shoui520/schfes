#ifndef SIF_JNI_UTF_H
#define SIF_JNI_UTF_H
#include <stddef.h>
#include <stdint.h>
/* JNI modified UTF-8 encodes NUL as C0 80 and each surrogate separately. */
static inline size_t sif_mutf8_encode(const uint16_t *src,size_t count,unsigned char *out) {
    size_t n=0;
    for(size_t i=0;i<count;i++) {
        unsigned c=src[i];
        if(c && c<128){if(out)out[n]=c;n++;}
        else if(c<2048){if(out){out[n]=0xc0|(c>>6);out[n+1]=0x80|(c&63);}n+=2;}
        else {if(out){out[n]=0xe0|(c>>12);out[n+1]=0x80|((c>>6)&63);out[n+2]=0x80|(c&63);}n+=3;}
    }
    return n;
}
static inline size_t sif_mutf8_decode(const unsigned char *src,size_t bytes,uint16_t *out) {
    size_t i=0,n=0;
    while(i<bytes) {
        unsigned c=src[i++],value=0xfffd;
        if(c<128)value=c;
        else if((c&0xe0)==0xc0 && i<bytes && (src[i]&0xc0)==0x80) {
            unsigned v=((c&31)<<6)|(src[i]&63);
            if(v>=128 || (c==0xc0 && src[i]==0x80)){value=v;i++;}
        } else if((c&0xf0)==0xe0 && i+1<bytes && (src[i]&0xc0)==0x80 && (src[i+1]&0xc0)==0x80) {
            unsigned v=((c&15)<<12)|((src[i]&63)<<6)|(src[i+1]&63);
            if(v>=2048){value=v;i+=2;}
        }
        if(out)out[n]=value;n++;
    }
    return n;
}
#endif
