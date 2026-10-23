#include "net_bridge.h"
/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 * Copyright (C) 2026      Ellie J Turner
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  dynlib.c
 * @brief Resolving dynamic imports of the .so.
 */

#include <psp2/kernel/clib.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>
#include <locale.h>
#include <poll.h>

#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <so_util/so_util.h>
#include <utime.h>

#include "utils/glutil.h"
#include "utils/utils.h"
#include "utils/logger.h"

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "reimpl/errno.h"
#include "reimpl/io.h"
#include "reimpl/log.h"
#include "reimpl/mem.h"
#include "reimpl/pthr.h"
#include "reimpl/sys.h"
#include "reimpl/egl.h"
#include "reimpl/time64.h"


#ifdef NDK_PORT
#include <falso_ndk/FalsoNDK.h>
#endif

const unsigned int __page_size = PAGE_SIZE;

extern void * _ZNSt9exceptionD2Ev;
extern void * _ZSt17__throw_bad_allocv;
extern void * _ZSt9terminatev;
extern void * _ZdaPv;
extern void * _ZdlPv;
extern void * _Znaj;
extern void * __cxa_allocate_exception;
extern void * __cxa_begin_catch;
extern void * __cxa_end_catch;
extern void * __cxa_free_exception;
extern void * __cxa_rethrow;
extern void * __cxa_throw;
extern void * __gxx_personality_v0;
extern void *_ZNSt8bad_castD1Ev;
extern void *_ZTISt8bad_cast;
extern void *_ZTISt9exception;
extern void *_ZTVN10__cxxabiv117__class_type_infoE;
extern void *_ZTVN10__cxxabiv120__si_class_type_infoE;
extern void *_ZTVN10__cxxabiv121__vmi_class_type_infoE;
extern void *_Znwj;
extern void *__aeabi_atexit;
extern void *__aeabi_d2lz;
extern void *__aeabi_d2ulz;
extern void *__aeabi_dadd;
extern void *__aeabi_dcmpgt;
extern void *__aeabi_dcmplt;
extern void *__aeabi_ddiv;
extern void *__aeabi_dmul;
extern void *__aeabi_f2lz;
extern void *__aeabi_f2ulz;
extern void *__aeabi_i2d;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_l2d;
extern void *__aeabi_l2f;
extern void *__aeabi_ldivmod;
extern void *__aeabi_memclr;
extern void *__aeabi_memcpy;
extern void *__aeabi_memmove;
extern void *__aeabi_memset4;
extern void *__aeabi_memset8;
extern void *__aeabi_memset;
extern void *__aeabi_ui2d;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__aeabi_ul2d;
extern void *__aeabi_ul2f;
extern void *__aeabi_uldivmod;
extern void *__aeabi_unwind_cpp_pr0;
extern void *__aeabi_unwind_cpp_pr1;
extern void *__cxa_atexit;
extern void *__cxa_call_unexpected;
extern void *__cxa_finalize;
extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;
extern void *__cxa_pure_virtual;
extern void *__gnu_ldivmod_helper;
extern void *__gnu_unwind_frame;
extern void *__srget;
extern void *__stack_chk_guard;
extern void *__swbuf;

extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;
extern const short *BIONIC_toupper_tab_;

#include "stdio_bridge.h"

void *dlsym_soloader(void * handle, const char * symbol);


#include "boundary.h"
void sif_glLinkProgram(unsigned);
GLenum sif_glGetError(void);
void sif_glPixelStorei(GLenum,GLint);
void sif_glTexSubImage2D(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*);
void sif_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
void sif_glBindAttribLocation(GLuint,GLuint,const GLchar*);
void sif_glVertexAttribPointer(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
void sif_glUniformMatrix4fv(GLint,GLsizei,GLboolean,const GLfloat*);
void sif_glDrawElements(GLenum,GLsizei,GLenum,const void*);
void sif_glBindFramebuffer(GLenum,GLuint);
#include "sif.h"
#include "compat.h"

so_default_dynlib default_dynlib[] = {
    {"__assert2", (uintptr_t)&sif_assert},
    {"__gnu_Unwind_Find_exidx", (uintptr_t)&sif_unwind},
    {"dlopen", (uintptr_t)&sif_dlopen},
    {"dlclose", (uintptr_t)&sif_dlclose},
    {"dlerror", (uintptr_t)&sif_dlerror},
    {"dladdr", (uintptr_t)&sif_dladdr},
    {"fchown", (uintptr_t)&sif_fchown},
    {"geteuid", (uintptr_t)&sif_geteuid},
    {"getpwuid", (uintptr_t)&sif_getpwuid},
    {"getrusage", (uintptr_t)&sif_getrusage},
    {"gettid", (uintptr_t)&sif_gettid},
    {"alarm", (uintptr_t)&sif_alarm},
    {"if_nametoindex", (uintptr_t)&sif_if_nametoindex},
    {"statfs", (uintptr_t)&sif_statfs},
    {"sysconf", (uintptr_t)&sif_sysconf},
    {"umask", (uintptr_t)&sif_umask},
    {"utimes", (uintptr_t)&sif_utimes},
    {"sigsetjmp", (uintptr_t)&sif_sigsetjmp},
    {"siglongjmp", (uintptr_t)&sif_siglongjmp},
    {"glCompressedTexSubImage2D", (uintptr_t)&sif_compressed_sub},
    {"glVertexAttrib1fv", (uintptr_t)&glVertexAttrib1fv},
    {"glVertexAttrib2fv", (uintptr_t)&glVertexAttrib2fv},
    {"glVertexAttrib3fv", (uintptr_t)&glVertexAttrib3fv},
    {"inet_addr", (uintptr_t)&inet_addr},
    {"mallinfo", (uintptr_t)&mallinfo},
    {"zlibVersion", (uintptr_t)&zlibVersion},
    {"SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE},
    {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE},
    {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY},
    {"SL_IID_VOLUME", (uintptr_t)&SL_IID_VOLUME},
    {"__aeabi_memclr", (uintptr_t)&__aeabi_memclr},
    {"__aeabi_memclr4", (uintptr_t)&__aeabi_memclr},
    {"__aeabi_memclr8", (uintptr_t)&__aeabi_memclr},
    {"__aeabi_memcpy", (uintptr_t)&sceClibMemcpy},
    {"__aeabi_memcpy4", (uintptr_t)&sceClibMemcpy},
    {"__aeabi_memcpy8", (uintptr_t)&sceClibMemcpy},
    {"__aeabi_memmove", (uintptr_t)&__aeabi_memmove},
    {"__aeabi_memmove4", (uintptr_t)&__aeabi_memmove},
    {"__aeabi_memset", (uintptr_t)&__aeabi_memset},
    {"__aeabi_memset4", (uintptr_t)&__aeabi_memset4},
    {"__aeabi_memset8", (uintptr_t)&__aeabi_memset8},
    {"__cxa_atexit", (uintptr_t)&__cxa_atexit},
    {"__cxa_finalize", (uintptr_t)&__cxa_finalize},
    {"__errno", (uintptr_t)&__errno_soloader},
    {"__fpclassifyd", (uintptr_t)&sif___fpclassifyd},
    {"__isinf", (uintptr_t)&sif___isinf},
    {"__sF", (uintptr_t)&sif_android_stdio},
    {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail_soloader},
    {"__stack_chk_guard", (uintptr_t)&__stack_chk_guard},
    {"_ctype_", (uintptr_t)&BIONIC_ctype_},
    {"_tolower_tab_", (uintptr_t)&BIONIC_tolower_tab_},
    {"_toupper_tab_", (uintptr_t)&BIONIC_toupper_tab_},
    {"abort", (uintptr_t)&abort_soloader},
    {"accept", (uintptr_t)&sif_accept},
    {"access", (uintptr_t)&sif_access},
    {"acos", (uintptr_t)&sif_acos},
    {"acosf", (uintptr_t)&sif_acosf},
    {"adler32", (uintptr_t)&adler32},
    {"asin", (uintptr_t)&sif_asin},
    {"atan", (uintptr_t)&sif_atan},
    {"atan2", (uintptr_t)&sif_atan2},
    {"atan2f", (uintptr_t)&sif_atan2f},
    {"atoi", (uintptr_t)&atoi},
    {"basename", (uintptr_t)&basename},
    {"bind", (uintptr_t)&sif_bind},
    {"calloc", (uintptr_t)&calloc},
    {"ceil", (uintptr_t)&sif_ceil},
    {"ceilf", (uintptr_t)&sif_ceilf},
    {"clock", (uintptr_t)&clock_soloader},
    {"clock_gettime", (uintptr_t)&sif_clock_gettime},
    {"close", (uintptr_t)&close_soloader},
    {"closedir", (uintptr_t)&closedir_soloader},
    {"connect", (uintptr_t)&sif_connect},
    {"cos", (uintptr_t)&sif_cos},
    {"cosf", (uintptr_t)&sif_cosf},
    {"cosh", (uintptr_t)&sif_cosh},
    {"crc32", (uintptr_t)&crc32},
    {"deflate", (uintptr_t)&deflate},
    {"deflateEnd", (uintptr_t)&deflateEnd},
    {"deflateInit2_", (uintptr_t)&deflateInit2_},
    {"deflateInit_", (uintptr_t)&deflateInit_},
    {"deflateReset", (uintptr_t)&deflateReset},
    {"difftime", (uintptr_t)&sif_difftime},
    {"dlsym", (uintptr_t)&dlsym_soloader},
    {"exit", (uintptr_t)&exit_soloader},
    {"exp", (uintptr_t)&sif_exp},
    {"fclose", (uintptr_t)&fclose_soloader},
    {"fcntl", (uintptr_t)&fcntl_soloader},
    {"fdopen", (uintptr_t)&fdopen_soloader},
    {"feof", (uintptr_t)&sif_feof},
    {"fflush", (uintptr_t)&sif_fflush},
    {"fgetc", (uintptr_t)&sif_fgetc},
    {"fgetpos", (uintptr_t)&sif_fgetpos},
    {"fgets", (uintptr_t)&sif_fgets},
    {"floor", (uintptr_t)&sif_floor},
    {"floorf", (uintptr_t)&sif_floorf},
    {"fmax", (uintptr_t)&sif_fmax},
    {"fmaxf", (uintptr_t)&sif_fmaxf},
    {"fmin", (uintptr_t)&sif_fmin},
    {"fmod", (uintptr_t)&sif_fmod},
    {"fmodf", (uintptr_t)&sif_fmodf},
    {"fopen", (uintptr_t)&fopen_soloader},
    {"fprintf", (uintptr_t)&sif_fprintf},
    {"fputc", (uintptr_t)&sif_fputc},
    {"fputs", (uintptr_t)&sif_fputs},
    {"fread", (uintptr_t)&sif_fread},
    {"free", (uintptr_t)&free},
    {"freeaddrinfo", (uintptr_t)&sif_freeaddrinfo},
    {"freopen", (uintptr_t)&sif_freopen},
    {"frexp", (uintptr_t)&sif_frexp},
    {"fscanf", (uintptr_t)&sif_fscanf},
    {"fseek", (uintptr_t)&sif_fseek},
    {"fseeko", (uintptr_t)&sif_fseeko},
    {"fsetpos", (uintptr_t)&sif_fsetpos},
    {"fstat", (uintptr_t)&fstat_soloader},
    {"fsync", (uintptr_t)&fsync_soloader},
    {"ftell", (uintptr_t)&sif_ftell},
    {"ftello", (uintptr_t)&sif_ftello},
    {"ftruncate", (uintptr_t)&ftruncate_soloader},
    {"fwrite", (uintptr_t)&sif_fwrite},
    {"getaddrinfo", (uintptr_t)&sif_getaddrinfo},
    {"getc", (uintptr_t)&sif_getc},
    {"getcwd", (uintptr_t)&getcwd},
    {"getenv", (uintptr_t)&getenv_soloader},
    {"gethostbyname", (uintptr_t)&gethostbyname},
    {"getpeername", (uintptr_t)&sif_getpeername},
    {"getpid", (uintptr_t)&getpid},
    {"getsockname", (uintptr_t)&sif_getsockname},
    {"getsockopt", (uintptr_t)&sif_getsockopt},
    {"gettimeofday", (uintptr_t)&sif_gettimeofday},
    {"glActiveTexture", (uintptr_t)&glActiveTexture},
    {"glAttachShader", (uintptr_t)&glAttachShader},
    {"glBindAttribLocation", (uintptr_t)&sif_glBindAttribLocation},
    {"glBindBuffer", (uintptr_t)&glBindBuffer},
    {"glBindFramebuffer", (uintptr_t)&sif_glBindFramebuffer},
    {"glBindTexture", (uintptr_t)&glBindTexture},
    {"glBlendEquation", (uintptr_t)&glBlendEquation},
    {"glBlendFunc", (uintptr_t)&glBlendFunc},
    {"glBufferData", (uintptr_t)&glBufferData},
    {"glBufferSubData", (uintptr_t)&glBufferSubData},
    {"glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus},
    {"glClear", (uintptr_t)&glClear},
    {"glClearColor", (uintptr_t)&sif_glClearColor},
    {"glClearDepthf", (uintptr_t)&sif_glClearDepthf},
    {"glClearStencil", (uintptr_t)&glClearStencil},
    {"glCompileShader", (uintptr_t)&glCompileShader_soloader},
    {"glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D},
    {"glCopyTexSubImage2D", (uintptr_t)&glCopyTexSubImage2D},
    {"glCreateProgram", (uintptr_t)&glCreateProgram},
    {"glCreateShader", (uintptr_t)&glCreateShader},
    {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},
    {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},
    {"glDeleteProgram", (uintptr_t)&glDeleteProgram},
    {"glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers},
    {"glDeleteShader", (uintptr_t)&glDeleteShader},
    {"glDeleteTextures", (uintptr_t)&glDeleteTextures},
    {"glDepthFunc", (uintptr_t)&glDepthFunc},
    {"glDepthMask", (uintptr_t)&glDepthMask},
    {"glDepthRangef", (uintptr_t)&sif_glDepthRangef},
    {"glDisable", (uintptr_t)&glDisable},
    {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},
    {"glDrawElements", (uintptr_t)&sif_glDrawElements},
    {"glEnable", (uintptr_t)&glEnable},
    {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},
    {"glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer},
    {"glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D},
    {"glGenBuffers", (uintptr_t)&glGenBuffers},
    {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},
    {"glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers},
    {"glGenTextures", (uintptr_t)&glGenTextures},
    {"glGenerateMipmap", (uintptr_t)&glGenerateMipmap},
    {"glGetError", (uintptr_t)&sif_glGetError},
    {"glGetIntegerv", (uintptr_t)&glGetIntegerv},
    {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},
    {"glGetProgramiv", (uintptr_t)&glGetProgramiv},
    {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},
    {"glGetShaderiv", (uintptr_t)&glGetShaderiv},
    {"glGetString", (uintptr_t)&glGetString},
    {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},
    {"glLinkProgram", (uintptr_t)&sif_glLinkProgram},
    {"glPixelStorei", (uintptr_t)&sif_glPixelStorei},
    {"glReadPixels", (uintptr_t)&glReadPixels},
    {"glScissor", (uintptr_t)&glScissor},
    {"glShaderSource", (uintptr_t)&glShaderSource_soloader},
    {"glStencilFuncSeparate", (uintptr_t)&glStencilFuncSeparate},
    {"glTexImage2D", (uintptr_t)&sif_glTexImage2D},
    {"glTexParameteri", (uintptr_t)&glTexParameteri},
    {"glTexSubImage2D", (uintptr_t)&sif_glTexSubImage2D},
    {"glUniform1fv", (uintptr_t)&glUniform1fv},
    {"glUniform1i", (uintptr_t)&glUniform1i},
    {"glUniform1iv", (uintptr_t)&glUniform1iv},
    {"glUniform2fv", (uintptr_t)&glUniform2fv},
    {"glUniform3fv", (uintptr_t)&glUniform3fv},
    {"glUniform4fv", (uintptr_t)&glUniform4fv},
    {"glUniformMatrix2fv", (uintptr_t)&glUniformMatrix2fv},
    {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv},
    {"glUniformMatrix4fv", (uintptr_t)&sif_glUniformMatrix4fv},
    {"glUseProgram", (uintptr_t)&glUseProgram},
    {"glVertexAttrib4fv", (uintptr_t)&glVertexAttrib4fv},
    {"glVertexAttribPointer", (uintptr_t)&sif_glVertexAttribPointer},
    {"glViewport", (uintptr_t)&glViewport},
    {"gmtime", (uintptr_t)&sif_gmtime},
    {"gmtime_r", (uintptr_t)&sif_gmtime_r},
    {"inet_ntop", (uintptr_t)&sif_inet_ntop},
    {"inet_pton", (uintptr_t)&sif_inet_pton},
    {"inflate", (uintptr_t)&inflate},
    {"inflateEnd", (uintptr_t)&inflateEnd},
    {"inflateInit2_", (uintptr_t)&inflateInit2_},
    {"inflateInit_", (uintptr_t)&inflateInit_},
    {"inflateReset", (uintptr_t)&inflateReset},
    {"ioctl", (uintptr_t)&ioctl_soloader},
    {"isalpha", (uintptr_t)&isalpha},
    {"iscntrl", (uintptr_t)&iscntrl},
    {"islower", (uintptr_t)&islower},
    {"isnan", (uintptr_t)&sif_isnan},
    {"isnanf", (uintptr_t)&sif_isnanf},
    {"isprint", (uintptr_t)&isprint},
    {"ispunct", (uintptr_t)&ispunct},
    {"isspace", (uintptr_t)&isspace},
    {"isupper", (uintptr_t)&isupper},
    {"iswalpha", (uintptr_t)&iswalpha},
    {"iswcntrl", (uintptr_t)&iswcntrl},
    {"iswdigit", (uintptr_t)&iswdigit},
    {"iswlower", (uintptr_t)&iswlower},
    {"iswprint", (uintptr_t)&iswprint},
    {"iswpunct", (uintptr_t)&iswpunct},
    {"iswspace", (uintptr_t)&iswspace},
    {"iswupper", (uintptr_t)&iswupper},
    {"iswxdigit", (uintptr_t)&iswxdigit},
    {"isxdigit", (uintptr_t)&isxdigit},
    {"ldexp", (uintptr_t)&sif_ldexp},
    {"listen", (uintptr_t)&listen},
    {"localtime", (uintptr_t)&sif_localtime},
    {"log", (uintptr_t)&sif_math_log},
    {"log10", (uintptr_t)&sif_log10},
    {"logf", (uintptr_t)&sif_logf},
    {"longjmp", (uintptr_t)&longjmp},
    {"lrand48", (uintptr_t)&lrand48},
    {"lseek", (uintptr_t)&lseek_soloader},
    {"malloc", (uintptr_t)&malloc},
    {"memalign", (uintptr_t)&memalign},
    {"memchr", (uintptr_t)&memchr},
    {"memcmp", (uintptr_t)&memcmp},
    {"memcpy", (uintptr_t)&sceClibMemcpy},
    {"memmove", (uintptr_t)&memmove},
    {"memrchr", (uintptr_t)&memrchr},
    {"memset", (uintptr_t)&memset},
    {"mkdir", (uintptr_t)&sif_mkdir},
    {"mktime", (uintptr_t)&sif_mktime},
    {"mmap", (uintptr_t)&mmap},
    {"modf", (uintptr_t)&sif_modf},
    {"munmap", (uintptr_t)&munmap},
    {"open", (uintptr_t)&open_soloader},
    {"opendir", (uintptr_t)&opendir_soloader},
    {"poll", (uintptr_t)&sif_poll},
    {"pow", (uintptr_t)&sif_pow},
    {"printf", (uintptr_t)&sceClibPrintf},
    {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_soloader},
    {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_soloader},
    {"pthread_cond_init", (uintptr_t)&pthread_cond_init_soloader},
    {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_soloader},
    {"pthread_create", (uintptr_t)&pthread_create_soloader},
    {"pthread_getspecific", (uintptr_t)&pthread_getspecific},
    {"pthread_join", (uintptr_t)&pthread_join_soloader},
    {"pthread_key_create", (uintptr_t)&pthread_key_create},
    {"pthread_key_delete", (uintptr_t)&pthread_key_delete},
    {"pthread_kill", (uintptr_t)&pthread_kill_soloader},
    {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_soloader},
    {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_soloader},
    {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_soloader},
    {"pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_soloader},
    {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_soloader},
    {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy_soloader},
    {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_soloader},
    {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_soloader},
    {"pthread_setspecific", (uintptr_t)&pthread_setspecific},
    {"putc", (uintptr_t)&sif_putc},
    {"putchar", (uintptr_t)&putchar},
    {"puts", (uintptr_t)&puts},
    {"qsort", (uintptr_t)&qsort},
    {"raise", (uintptr_t)&raise},
    {"read", (uintptr_t)&read_soloader},
    {"readdir", (uintptr_t)&readdir_soloader},
    {"realloc", (uintptr_t)&realloc},
    {"recv", (uintptr_t)&sif_recv},
    {"remove", (uintptr_t)&sif_remove},
    {"rename", (uintptr_t)&sif_rename},
    {"rmdir", (uintptr_t)&sif_rmdir},
    {"roundf", (uintptr_t)&sif_roundf},
    {"send", (uintptr_t)&sif_send},
    {"sendto", (uintptr_t)&sif_sendto},
    {"setjmp", (uintptr_t)&setjmp},
    {"setlocale", (uintptr_t)&setlocale},
    {"setsockopt", (uintptr_t)&sif_setsockopt},
    {"setvbuf", (uintptr_t)&sif_setvbuf},
    {"sigaction", (uintptr_t)&sigaction},
    {"sin", (uintptr_t)&sif_sin},
    {"sinf", (uintptr_t)&sif_sinf},
    {"sinh", (uintptr_t)&sif_sinh},
    {"slCreateEngine", (uintptr_t)&slCreateEngine},
    {"sleep", (uintptr_t)&sleep},
    {"snprintf", (uintptr_t)&snprintf},
    {"socket", (uintptr_t)&sif_socket},
    {"sprintf", (uintptr_t)&sprintf},
    {"sqrt", (uintptr_t)&sif_sqrt},
    {"sqrtf", (uintptr_t)&sif_sqrtf},
    {"srand48", (uintptr_t)&srand48},
    {"sscanf", (uintptr_t)&sscanf},
    {"stat", (uintptr_t)&stat_soloader},
    {"strcasecmp", (uintptr_t)&strcasecmp},
    {"strcat", (uintptr_t)&strcat},
    {"strchr", (uintptr_t)&strchr},
    {"strcmp", (uintptr_t)&strcmp},
    {"strcoll", (uintptr_t)&strcoll},
    {"strcpy", (uintptr_t)&strcpy},
    {"strdup", (uintptr_t)&strdup},
    {"strerror", (uintptr_t)&strerror_soloader},
    {"strerror_r", (uintptr_t)&strerror_r_soloader},
    {"strftime", (uintptr_t)&strftime},
    {"strlen", (uintptr_t)&strlen},
    {"strncat", (uintptr_t)&strncat},
    {"strncmp", (uintptr_t)&strncmp},
    {"strncpy", (uintptr_t)&strncpy},
    {"strpbrk", (uintptr_t)&strpbrk},
    {"strrchr", (uintptr_t)&strrchr},
    {"strspn", (uintptr_t)&strspn},
    {"strstr", (uintptr_t)&strstr},
    {"strtod", (uintptr_t)&sif_strtod},
    {"strtok_r", (uintptr_t)&strtok_r},
    {"strtol", (uintptr_t)&strtol},
    {"strtoll", (uintptr_t)&strtoll},
    {"strtoul", (uintptr_t)&strtoul},
    {"tan", (uintptr_t)&sif_tan},
    {"tanh", (uintptr_t)&sif_tanh},
    {"time", (uintptr_t)&sif_time},
    {"tmpfile", (uintptr_t)&tmpfile},
    {"tmpnam", (uintptr_t)&tmpnam},
    {"tolower", (uintptr_t)&tolower},
    {"toupper", (uintptr_t)&toupper},
    {"towlower", (uintptr_t)&towlower},
    {"towupper", (uintptr_t)&towupper},
    {"truncf", (uintptr_t)&sif_truncf},
    {"ungetc", (uintptr_t)&sif_ungetc},
    {"unlink", (uintptr_t)&sif_unlink},
    {"usleep", (uintptr_t)&usleep},
    {"vsprintf", (uintptr_t)&vsprintf},
    {"wcscmp", (uintptr_t)&wcscmp},
    {"wcslen", (uintptr_t)&wcslen},
    {"wcsncpy", (uintptr_t)&wcsncpy},
    {"wmemcpy", (uintptr_t)&wmemcpy},
    {"wmemmove", (uintptr_t)&wmemmove},
    {"wmemset", (uintptr_t)&wmemset},
    {"write", (uintptr_t)&write_soloader},
};

void *dlsym_soloader(void *handle, const char *symbol) {
    for (unsigned i=0; i<sizeof(default_dynlib)/sizeof(default_dynlib[0]); ++i)
        if (!strcmp(symbol, default_dynlib[i].symbol)) return (void *)default_dynlib[i].func;
    sif_log("Unresolved dlsym: %s", symbol);
    return NULL;
}
int resolve_imports(so_module *mod) {
    int missing=0;
    for (unsigned i=0;i<mod->num_dynsym;i++) {
        Elf32_Sym *s=&mod->dynsym[i];
        if (s->st_shndx!=SHN_UNDEF || !s->st_name) continue;
        const char *name=mod->dynstr+s->st_name;
        if (!dlsym_soloader(NULL,name)) ++missing;
    }
    if (missing) { sif_log("STOP: %d unresolved imports",missing); return -1; }
    so_resolve(mod,default_dynlib,sizeof(default_dynlib),0);
    return 0;
}
