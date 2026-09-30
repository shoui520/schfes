#include "crypto.h"
#include "string_hash.h"
#include "sif.h"
#include <psp2/kernel/rng.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static X509 *apk_certificate;
static char *apk_certificate_chars;
jobject sif_sha512(jmethodID id,va_list a) {
    jstring string=va_arg(a,jstring);
    if(!string)sif_stop("Null SHA512 string");
    jsize length=jni->GetStringLength(&jni,string);
    const jchar *text=jni->GetStringChars(&jni,string,NULL);
    if(length<0 || !text)sif_stop("SHA512 string conversion failed");
    unsigned char digest[64];sif_sha512_utf16(text,length,digest);
    jni->ReleaseStringChars(&jni,string,text);
    jbyteArray result=jni->NewByteArray(&jni,sizeof(digest));
    if(!result)sif_stop("SHA512 result allocation failed");
    jni->SetByteArrayRegion(&jni,result,0,sizeof(digest),(const jbyte*)digest);
    static unsigned calls;
    if(calls++<4)sif_log("JNI SHA512: %d UTF-16 units, 64-byte digest",length);
    return result;
}
static X509 *load_apk_certificate(void) {
    if(apk_certificate)return apk_certificate;
    FILE *file=fopen(DATA_PATH "apk-signing-cert.der","rb");
    if(!file)sif_stop("APK signing certificate missing");
    unsigned char bytes[16384],digest[32];char hash[65];
    size_t size=fread(bytes,1,sizeof(bytes),file);
    int bad=ferror(file) || size==sizeof(bytes);fclose(file);
    if(bad || !size)sif_stop("APK signing certificate read failed");
    SHA256(bytes,size,digest);
    for(unsigned i=0;i<32;i++)snprintf(hash+i*2,3,"%02x",digest[i]);
    // SDK_INT=19 selects the APK's v1 signer. Its newer v3 signing identity
    // differs; using that newer certificate would change the DEX contract.
    if(strcmp(hash,"073146bed1b213e60057da4c054fa19c41aa9977e2a8e82693cb8454e6c18e71"))
        sif_stop("Unsupported APK signing certificate");
    const unsigned char *cursor=bytes;
    apk_certificate=d2i_X509(NULL,&cursor,size);
    if(!apk_certificate || cursor!=bytes+size)sif_stop("APK X.509 decoding failed");
    apk_certificate_chars=malloc(size*2+1);
    if(!apk_certificate_chars)sif_stop("APK certificate string allocation failed");
    for(size_t i=0;i<size;i++)snprintf(apk_certificate_chars+i*2,3,"%02x",bytes[i]);
    return apk_certificate;
}
jobject sif_apk_signature_chars(jmethodID id,va_list a) {
    // PFInterface.getString returns "null" until calAPKSignature initializes
    // its cached signer; afterwards Signature.toCharsString is lowercase hex.
    sif_log("JNI APK signer chars: %s",apk_certificate_chars?"initialized":"not initialized");
    return jni->NewStringUTF(&jni,apk_certificate_chars?apk_certificate_chars:"null");
}
jobject sif_apk_signature(jmethodID id,va_list a) {
    X509 *certificate=load_apk_certificate();
    int length=i2d_X509(certificate,NULL);
    if(length<=0)return jni->NewStringUTF(&jni,"ERROR");
    JavaDynArray *encoded=jda_alloc(length,FIELD_TYPE_BYTE);
    if(!encoded)sif_stop("APK certificate byte-array allocation failed");
    unsigned char *cursor=encoded->array;
    if(i2d_X509(certificate,&cursor)!=length) {
        jda_free(encoded);return jni->NewStringUTF(&jni,"ERROR");
    }
    // Verified classes9.dex calls getEncoded().toString().getBytes(UTF_8),
    // not a certificate digest. Object identity is implementation-defined;
    // the FalsoJNI array object's address supplies its stable identity hash.
    char identity[16],result[48];
    int size=snprintf(identity,sizeof(identity),"[B@%x",(unsigned)(uintptr_t)encoded);
    for(int i=0;i<size;i++)snprintf(result+i*3,4,"%02X:",(unsigned char)identity[i]);
    result[size*3-1]=0;jda_free(encoded);
    static unsigned calls;
    if(calls++<2)sif_log("JNI APK signature: validated v1 certificate, encoded array identity");
    return jni->NewStringUTF(&jni,result);
}
jboolean sif_random_bytes(jmethodID id,va_list a) {
    jbyteArray out=va_arg(a,jbyteArray);
    if(!out)sif_stop("Null secure-random byte array");
    jsize n=jni->GetArrayLength(&jni,out);
    if(n<0)sif_stop("Invalid secure-random byte array length");
    if(!n)return JNI_TRUE;
    unsigned char *buffer=malloc(n);
    if(!buffer)sif_stop("Secure-random allocation failed");
    // Vita's kernel RNG accepts at most 64 bytes per call. Fill a staging
    // buffer so a kernel error never commits a partially populated Java array.
    for(jsize offset=0;offset<n;) {
        unsigned chunk=n-offset>64?64:n-offset;
        int result=sceKernelGetRandomNumber(buffer+offset,chunk);
        if(result<0) {
            memset(buffer,0,n);free(buffer);
            sif_log("Secure-random kernel error %08x",(unsigned)result);
            return JNI_FALSE;
        }
        offset+=chunk;
    }
    jni->SetByteArrayRegion(&jni,out,0,n,(const jbyte*)buffer);
    memset(buffer,0,n);free(buffer);
    static unsigned calls;
    if(calls++<4)sif_log("Secure-random byte array populated: %d bytes",n);
    return JNI_TRUE;
}

#include "rsa_verify.h"
jboolean sif_public_key_verify(jmethodID id,va_list args) {
    jbyteArray data=va_arg(args,jbyteArray),signature=va_arg(args,jbyteArray),key=va_arg(args,jbyteArray);
    if(!data || !signature || !key)return JNI_FALSE;
    jsize n=jni->GetArrayLength(&jni,data),sn=jni->GetArrayLength(&jni,signature),kn=jni->GetArrayLength(&jni,key);
    jbyte *d=jni->GetByteArrayElements(&jni,data,NULL),*s=jni->GetByteArrayElements(&jni,signature,NULL),*k=jni->GetByteArrayElements(&jni,key,NULL);
    int valid=d && s && k && sif_rsa_sha1_verify(d,n,s,sn,k,kn);
    if(d)jni->ReleaseByteArrayElements(&jni,data,d,JNI_ABORT);
    if(s)jni->ReleaseByteArrayElements(&jni,signature,s,JNI_ABORT);
    if(k)jni->ReleaseByteArrayElements(&jni,key,k,JNI_ABORT);
    sif_log("JNI RSA SHA1 verification: %s",valid?"valid":"invalid");
    return valid?JNI_TRUE:JNI_FALSE;
}

jint sif_public_key_encrypt(jmethodID id,va_list args) {
    jbyteArray src=va_arg(args,jbyteArray),key=va_arg(args,jbyteArray),out=va_arg(args,jbyteArray);
    if(!src || !key || !out)return -1;
    jsize n=jni->GetArrayLength(&jni,src),kn=jni->GetArrayLength(&jni,key),capacity=jni->GetArrayLength(&jni,out);
    jbyte *data=jni->GetByteArrayElements(&jni,src,NULL),*pem=jni->GetByteArrayElements(&jni,key,NULL);
    unsigned char entropy[32],*encrypted=malloc(capacity?capacity:1);int result=-1;
    if(data && pem && encrypted && sceKernelGetRandomNumber(entropy,sizeof(entropy))>=0)
        result=sif_rsa_pkcs1_encrypt(data,n,pem,kn,entropy,encrypted,capacity);
    if(result>=0)jni->SetByteArrayRegion(&jni,out,0,result,(const jbyte*)encrypted);
    memset(entropy,0,sizeof(entropy));if(encrypted){memset(encrypted,0,capacity);free(encrypted);}
    if(data)jni->ReleaseByteArrayElements(&jni,src,data,JNI_ABORT);
    if(pem)jni->ReleaseByteArrayElements(&jni,key,pem,JNI_ABORT);
    sif_log("JNI RSA PKCS1 encryption result=%d",result);return result;
}

#include "aes_cbc.h"
static jint aes_callback(va_list args,int decrypt) {
    jbyteArray src=va_arg(args,jbyteArray),key=va_arg(args,jbyteArray),out=va_arg(args,jbyteArray);
    if(!src || !key || !out)return -9;
    jsize n=jni->GetArrayLength(&jni,src),kn=jni->GetArrayLength(&jni,key),cap=jni->GetArrayLength(&jni,out);
    jbyte *data=jni->GetByteArrayElements(&jni,src,NULL),*k=jni->GetByteArrayElements(&jni,key,NULL);
    unsigned char iv[16]={0},*output=malloc(cap?cap:1);int result=-1;
    if(data && k && output && (decrypt || sceKernelGetRandomNumber(iv,sizeof(iv))>=0))
        result=sif_aes128_cbc(decrypt,(void*)data,n,(void*)k,kn,output,cap,iv);
    if(result>=0)jni->SetByteArrayRegion(&jni,out,0,result,(void*)output);
    if(output){OPENSSL_cleanse(output,cap);free(output);}
    if(data)jni->ReleaseByteArrayElements(&jni,src,data,JNI_ABORT);
    if(k)jni->ReleaseByteArrayElements(&jni,key,k,JNI_ABORT);
    sif_log("JNI AES CBC %s result=%d",decrypt?"decrypt":"encrypt",result);return result;
}
jint sif_aes_encrypt(jmethodID id,va_list args){return aes_callback(args,0);}
jint sif_aes_decrypt(jmethodID id,va_list args){return aes_callback(args,1);}
