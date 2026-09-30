#include "logger.h"
#include "sif.h"
#include <stdarg.h>
#include <stdio.h>
void _log_print(int t,const char *fmt,...) {
#ifndef SIF_VERBOSE_COMPAT_LOG
    if(t==LT_DEBUG)return;
#endif
    char b[2048]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof(b),fmt,a); va_end(a); sif_log("%d: %s",t,b); if(t==LT_FATAL)sif_stop("Fatal compatibility-layer error"); }
