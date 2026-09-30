#pragma once
#include <falso_jni/FalsoJNI.h>
#include <stdarg.h>
#define JNI_PLATFORM(id,name,sig,type,ret,table) ret sif_jp_##name(jmethodID,va_list);
#include "java_platform_methods.inc"
#undef JNI_PLATFORM
void sif_java_platform_init(void);
void sif_java_platform_frame(void);
void sif_java_platform_draw(void);
int sif_java_platform_touch(int x,int y);
int sif_java_platform_modal(void);
void sif_overlay_begin(void);
void sif_overlay_bitmap(unsigned *texture,const unsigned *pixels,int w,int h,int x,int y);
void sif_overlay_end(void);
void sif_overlay_delete(unsigned texture);
