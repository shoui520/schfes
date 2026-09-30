#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/sha.h>

// String.getBytes(StandardCharsets.UTF_8): preserve NUL, combine surrogate
// pairs, and replace an unpaired UTF-16 surrogate with the encoder's '?'.
static inline void sif_sha512_utf16(const uint16_t *text,size_t length,unsigned char digest[64]) {
    SHA512_CTX context;SHA512_Init(&context);
    for(size_t i=0;i<length;i++) {
        uint32_t code=text[i];unsigned char bytes[4];unsigned size;
        if(code>=0xd800 && code<=0xdbff) {
            if(i+1<length && text[i+1]>=0xdc00 && text[i+1]<=0xdfff)
                code=0x10000+((code-0xd800)<<10)+(text[++i]-0xdc00);
            else code='?';
        } else if(code>=0xdc00 && code<=0xdfff)code='?';
        if(code<0x80){bytes[0]=code;size=1;}
        else if(code<0x800){bytes[0]=0xc0|(code>>6);bytes[1]=0x80|(code&63);size=2;}
        else if(code<0x10000){bytes[0]=0xe0|(code>>12);bytes[1]=0x80|((code>>6)&63);bytes[2]=0x80|(code&63);size=3;}
        else {bytes[0]=0xf0|(code>>18);bytes[1]=0x80|((code>>12)&63);bytes[2]=0x80|((code>>6)&63);bytes[3]=0x80|(code&63);size=4;}
        SHA512_Update(&context,bytes,size);
    }
    SHA512_Final(digest,&context);
}
