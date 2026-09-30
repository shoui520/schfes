#pragma once
#include <falso_jni/FalsoJNI.h>
#include <stdarg.h>
jobject sif_paint_create(jmethodID,va_list);
jobject sif_font_metrics(jmethodID,va_list);
void sif_font_size(jmethodID,va_list);
void sif_font_antialias(jmethodID,va_list);
jfloat sif_font_measure(jmethodID,va_list);
void sif_draw_text(jmethodID,va_list);
jfloat sif_font_field(JNIEnv*,jobject,jfieldID);
void sif_font_widget(jobject paint,const char *text,unsigned *pixels,int w,int h,int alignment,int enabled);
