#include "sif.h"
#include <falso_jni/FalsoJNI.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
static pthread_mutex_t preferences_lock=PTHREAD_MUTEX_INITIALIZER;
static void key_component(SHA256_CTX *ctx,jstring s) {
    static const jchar null_text[]={'n','u','l','l'};
    if(!s){SHA256_Update(ctx,null_text,sizeof(null_text));return;}
    jsize n=jni->GetStringLength(&jni,s);const jchar *p=jni->GetStringChars(&jni,s,NULL);
    if(!p)sif_stop("Preference key allocation failed");
    SHA256_Update(ctx,p,(size_t)n*2);jni->ReleaseStringChars(&jni,s,p);
}
static void key_path(char *path,jstring service,jstring key) {
    SHA256_CTX ctx;unsigned char digest[32];jchar c='[';
    SHA256_Init(&ctx);SHA256_Update(&ctx,&c,2);key_component(&ctx,service);
    c=']';SHA256_Update(&ctx,&c,2);key_component(&ctx,key);SHA256_Final(digest,&ctx);
    strcpy(path,DATA_PATH "preferences/");size_t n=strlen(path);
    for(unsigned i=0;i<32;i++)sprintf(path+n+2*i,"%02x",digest[i]);
}
jobject sif_keychain_get(jmethodID id,va_list args) {
    jstring service=va_arg(args,jstring),key=va_arg(args,jstring);char path[160];key_path(path,service,key);
    pthread_mutex_lock(&preferences_lock);FILE *f=fopen(path,"rb");
    if(!f){int e=errno;pthread_mutex_unlock(&preferences_lock);if(e!=ENOENT)sif_stop("Preference read failed");return jni->NewStringUTF(&jni,"");}
    char magic[8];uint32_t count;
    if(fread(magic,1,8,f)!=8 || memcmp(magic,"SIFPREF1",8) || fread(&count,4,1,f)!=1 || count>8*1024*1024)
        sif_stop("Preference file invalid");
    jchar *value=malloc(count?count*2:2);if(!value)sif_stop("Preference allocation failed");
    if(fread(value,2,count,f)!=count || fgetc(f)!=EOF || ferror(f))sif_stop("Preference file truncated");
    fclose(f);pthread_mutex_unlock(&preferences_lock);
    jobject result=jni->NewString(&jni,value,count);free(value);return result;
}
static jboolean keychain_write(va_list args,int deleting) {
    jstring service=va_arg(args,jstring),key=va_arg(args,jstring);
    jstring value=deleting?NULL:va_arg(args,jstring);char path[160],tmp[168];key_path(path,service,key);
    pthread_mutex_lock(&preferences_lock);
    if(!value) {
        int r=remove(path),e=errno;pthread_mutex_unlock(&preferences_lock);
        if(r && e!=ENOENT)sif_stop("Preference removal failed");return JNI_TRUE;
    }
    if(mkdir(DATA_PATH "preferences",0700)<0 && errno!=EEXIST)sif_stop("Preference directory failed");
    jsize n=jni->GetStringLength(&jni,value);const jchar *p=jni->GetStringChars(&jni,value,NULL);
    if(!p || n>8*1024*1024)sif_stop("Preference value invalid");
    snprintf(tmp,sizeof(tmp),"%s.tmp",path);FILE *f=fopen(tmp,"wb");if(!f)sif_stop("Preference write failed");
    uint32_t count=n;
    int ok=fwrite("SIFPREF1",1,8,f)==8 && fwrite(&count,4,1,f)==1 && fwrite(p,2,count,f)==count && fflush(f)==0 && fsync(fileno(f))==0;
    if(fclose(f))ok=0;jni->ReleaseStringChars(&jni,value,p);
    if(!ok || rename(tmp,path)<0)sif_stop("Preference commit failed");
    pthread_mutex_unlock(&preferences_lock);return JNI_TRUE;
}
jboolean sif_keychain_set(jmethodID id,va_list args){return keychain_write(args,0);}
jboolean sif_keychain_delete(jmethodID id,va_list args){return keychain_write(args,1);}
