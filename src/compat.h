#pragma once
#include <stdint.h>
#include <stddef.h>
struct sif_time32 { int32_t sec, sub; };
void sif_assert(const char*,int,const char*,const char*) __attribute__((noreturn));
uintptr_t sif_unwind(uintptr_t,int*);
void *sif_dlopen(const char*,int);
int sif_dlclose(void*);
char *sif_dlerror(void);
int sif_dladdr(const void*,void*);
int sif_fchown(int,unsigned,unsigned);
int sif_geteuid(void);
void *sif_getpwuid(unsigned);
int sif_getrusage(int,void*);
int sif_gettid(void);
unsigned sif_alarm(unsigned);
unsigned sif_if_nametoindex(const char*);
int sif_statfs(const char*,void*);
long sif_sysconf(int);
unsigned sif_umask(unsigned);
int sif_utimes(const char*,const struct sif_time32*);
int sif_gettimeofday(struct sif_time32*,void*);
int sif_clock_gettime(int,struct sif_time32*);
int32_t sif_time(int32_t*);
struct tm *sif_gmtime(const int32_t*);
struct tm *sif_gmtime_r(const int32_t*,struct tm*);
struct tm *sif_localtime(const int32_t*);
int32_t sif_mktime(struct tm*);
int sif_sigsetjmp(void*,int) __attribute__((naked,returns_twice));
void sif_siglongjmp(void*,int) __attribute__((naked,noreturn));
void sif_compressed_sub(unsigned,int,int,int,int,int,unsigned,int,const void*);

extern unsigned sif_file_mask;
int sif_mkdir(const char*,unsigned);
const char *sif_io_path(const char*);
struct sif_asset_ref;
/* Physical path plus the asset reference used for assets.bin lookups. */
const char *sif_io_target(const char*,struct sif_asset_ref*);
int sif_access(const char*,int);
int sif_unlink(const char*);
int sif_rename(const char*,const char*);

int sif_remove(const char*);
int sif_rmdir(const char*);
