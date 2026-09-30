#ifndef SIF_NET_ADDRESS_H
#define SIF_NET_ADDRESS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Android ARM sockaddr has a 16-bit family; Vita has length/family bytes.
 * IPv6 additionally has a Vita virtual-port field before scope_id. */
static inline int sif_address_to_vita(const void *src,size_t n,void *dst) {
    if(!src || n<2)return -1;
    uint16_t family;memcpy(&family,src,2);
    const unsigned char *s=src;unsigned char *d=dst;
    memset(d,0,32);
    if(family==2 && n>=16) {
        d[0]=16;d[1]=2;memcpy(d+2,s+2,6);return 16;
    }
    if(family==10 && n>=28) {
        d[0]=32;d[1]=24;memcpy(d+2,s+2,22);memcpy(d+28,s+24,4);return 32;
    }
    return -1;
}
static inline int sif_address_to_android(const void *src,size_t n,void *dst) {
    if(!src || n<2)return -1;
    const unsigned char *s=src;unsigned char *d=dst;
    memset(d,0,28);
    uint16_t family;
    if(s[1]==2 && n>=16) {
        family=2;memcpy(d,&family,2);memcpy(d+2,s+2,6);return 16;
    }
    if(s[1]==24 && n>=32) {
        family=10;memcpy(d,&family,2);memcpy(d+2,s+2,22);memcpy(d+24,s+28,4);return 28;
    }
    return -1;
}
#endif
