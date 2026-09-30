#ifndef SIF_RSA_VERIFY_H
#define SIF_RSA_VERIFY_H
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <limits.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
static inline int sif_rsa_sha1_verify(const void *data,size_t size,const void *signature,size_t sig_size,const void *pem,size_t pem_size) {
    if(!data || !signature || !pem || pem_size>INT_MAX)return 0;
    BIO *bio=BIO_new_mem_buf(pem,(int)pem_size);if(!bio)return 0;
    EVP_PKEY *key=PEM_read_bio_PUBKEY(bio,NULL,NULL,NULL);BIO_free(bio);
    if(!key)return 0;
    EVP_MD_CTX *ctx=EVP_MD_CTX_create();int valid=0;
    if(ctx && EVP_PKEY_base_id(key)==EVP_PKEY_RSA && EVP_DigestVerifyInit(ctx,NULL,EVP_sha1(),NULL,key)==1 &&
       EVP_DigestVerifyUpdate(ctx,data,size)==1 && EVP_DigestVerifyFinal(ctx,signature,sig_size)==1)valid=1;
    EVP_MD_CTX_destroy(ctx);EVP_PKEY_free(key);return valid;
}
static inline int sif_rsa_pkcs1_encrypt(const void *data,size_t size,const void *pem,size_t pem_size,
    const unsigned char entropy[32],unsigned char *out,size_t capacity) {
    if(!data || !pem || pem_size>INT_MAX || size>INT_MAX || !out)return -1;
    BIO *bio=BIO_new_mem_buf(pem,(int)pem_size);if(!bio)return -1;
    EVP_PKEY *key=PEM_read_bio_PUBKEY(bio,NULL,NULL,NULL);BIO_free(bio);if(!key)return -1;
    RSA *rsa=EVP_PKEY_get1_RSA(key);EVP_PKEY_free(key);if(!rsa)return -1;
    int n=RSA_size(rsa),result=-1;
    if(n>=11 && size<=(size_t)(n-11) && capacity>=(size_t)n) {
        RAND_seed(entropy,32);
        result=RSA_public_encrypt(size,data,out,rsa,RSA_PKCS1_PADDING);
    }
    RSA_free(rsa);return result;
}
#endif
