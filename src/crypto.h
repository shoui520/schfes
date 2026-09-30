#pragma once
#include <falso_jni/FalsoJNI.h>
#include <stdarg.h>
jboolean sif_random_bytes(jmethodID,va_list);
jobject sif_apk_signature(jmethodID,va_list);
jobject sif_apk_signature_chars(jmethodID,va_list);
jobject sif_sha512(jmethodID,va_list);

jboolean sif_public_key_verify(jmethodID,va_list);
jint sif_public_key_encrypt(jmethodID,va_list);
jint sif_aes_encrypt(jmethodID,va_list);
jint sif_aes_decrypt(jmethodID,va_list);
