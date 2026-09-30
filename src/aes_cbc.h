#ifndef SIF_AES_CBC_H
#define SIF_AES_CBC_H
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
/* Android contract: first 16 key bytes, PKCS7 padding, IV prefixed to ciphertext. */
static inline int sif_aes128_cbc(int decrypt,const unsigned char *src,size_t n,const unsigned char *key,size_t key_n,
    unsigned char *out,size_t capacity,const unsigned char iv[16]) {
    if(!src || !key || !out)return -9;
    if(key_n<16)return -2;
    if(n>INT_MAX-32)return -6;
    if(decrypt && n<16)return -11;
    if(decrypt && (!(n-16) || (n-16)%16))return -6;
    size_t allocated=n+32;unsigned char *temp=malloc(allocated);if(!temp)return -1;
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();int result=-1,a=0,b=0;
    if(!ctx)goto done;
    if(EVP_CipherInit_ex(ctx,EVP_aes_128_cbc(),NULL,key,decrypt?src:iv,!decrypt)!=1){result=-5;goto done;}
    if(EVP_CipherUpdate(ctx,temp,&a,decrypt?src+16:src,decrypt?n-16:n)!=1){result=-6;goto done;}
    if(EVP_CipherFinal_ex(ctx,temp+a,&b)!=1){result=-7;goto done;}
    size_t size=a+b+(decrypt?0:16);
    if(size>capacity){result=-8;goto done;}
    if(!decrypt)memcpy(out,iv,16);
    memcpy(out+(decrypt?0:16),temp,a+b);result=size;
done:
    EVP_CIPHER_CTX_free(ctx);OPENSSL_cleanse(temp,allocated);free(temp);return result;
}
#endif
