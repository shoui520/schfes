/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"
#include "compat.h"
#include "stdio_bridge.h"
#include "asset_pack.h"
#include <errno.h>

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>
#ifdef SIF_PRIVATE_DIAGNOSTICS
#include <psp2/kernel/processmgr.h>
extern void sif_log(const char*,...);
#endif

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

FILE * fopen_soloader(const char * filename, const char * mode) {
#ifdef SIF_PRIVATE_DIAGNOSTICS
    uint64_t started=sceKernelGetProcessTimeWide();
#endif
    struct sif_asset_ref ref;
    filename=sif_io_target(filename,&ref);
    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

    FILE* ret;
    if (!sif_pack_fopen(&ref, mode, &ret)) {
        ret = fopen(filename, mode);
        /* Collapsed or pack-only parents have no loose directory yet. */
        if (!ret && errno == ENOENT && ref.root >= 0 && strpbrk(mode, "wa") &&
            sif_pack_make_parents(filename) == 0)
            ret = fopen(filename, mode);
        else if (!ret && errno == ENOENT && !strpbrk(mode, "wa+") && sif_asset_uncollapsed(&ref))
            ret = fopen(filename = sif_asset_uncollapsed(&ref), mode);
        if(ret && (!strcmp(mode,"r") || !strcmp(mode,"rb")) &&
           (!strncmp(filename,"ux0:/",5) || !strncmp(filename,"app0:/",6)))
            sif_prepare_read_file(ret);
    }

#ifdef SIF_PRIVATE_DIAGNOSTICS
    extern void sif_diagnostic_file(FILE*,const char*);
    if(ret)sif_diagnostic_file(ret,filename);
    uint64_t us=sceKernelGetProcessTimeWide()-started;
    if(us>100000)sif_log("SLOW fopen us=%llu path=%s",(unsigned long long)us,filename);
#endif
    if (ret)
        l_debug("fopen(%s, %s): %p", filename, mode, ret);
    else
        l_warn("fopen(%s, %s): %p", filename, mode, ret);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    struct sif_asset_ref ref;
    path=sif_io_target(path,&ref);
    if (strcmp(path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    } else if (strcmp(path, "/dev/urandom") == 0) {
        return open_soloader("app0:/urandom", oflag);
    }

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret;
    if (!sif_pack_open(&ref, oflag, &ret)) {
        ret = open(path, oflag, mode&~sif_file_mask);
        if (ret < 0 && errno == ENOENT && ref.root >= 0 && (oflag & O_CREAT) &&
            sif_pack_make_parents(path) == 0)
            ret = open(path, oflag, mode&~sif_file_mask);
        else if (ret < 0 && errno == ENOENT && (oflag & O_ACCMODE) == O_RDONLY && sif_asset_uncollapsed(&ref))
            ret = open(path = sif_asset_uncollapsed(&ref), oflag, mode&~sif_file_mask);
    }
    if (ret >= 0)
        l_debug("open(%s, %x): %i", path, oflag, ret);
    else {
        int saved_errno=errno;
        l_warn("open(%s, %x): %i errno=%d", path, oflag, ret,saved_errno);
        errno=saved_errno;
    }
    return ret;
}

static void pack_stat(int directory, uint32_t size, uint32_t mtime, stat64_bionic * buf) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_mode = directory ? S_IFDIR | 0777 : S_IFREG | 0666;
    st.st_nlink = 1;
    st.st_size = size;
    st.st_blksize = 16384;
    st.st_blocks = (size + 511) / 512;
    st.st_atime = st.st_mtime = st.st_ctime = mtime;
    stat_newlib_to_bionic(&st, buf);
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    if (sif_pack_is_fd(fd)) {
        uint32_t size, mtime;
        sif_pack_fstat(fd, &size, &mtime);
        pack_stat(0, size, mtime, buf);
        return 0;
    }
    struct stat st;
#ifdef SIF_PRIVATE_DIAGNOSTICS
    uint64_t started=sceKernelGetProcessTimeWide();
#endif
    int res = fstat(fd, &st);
#ifdef SIF_PRIVATE_DIAGNOSTICS
    uint64_t us=sceKernelGetProcessTimeWide()-started;
    if(us>100000)sif_log("SLOW fstat fd=%d us=%llu",fd,(unsigned long long)us);
#endif

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    struct sif_asset_ref ref;
    path=sif_io_target(path,&ref);
    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }
    struct sif_pack_stat packed;
    if (sif_pack_stat(&ref, &packed)) {
        pack_stat(packed.directory, packed.size, packed.mtime, buf);
        return 0;
    }

    struct stat st;
    int res = stat(path, &st);
    if (res < 0 && errno == ENOENT && sif_asset_uncollapsed(&ref))
        res = stat(sif_asset_uncollapsed(&ref), &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s): %i", path, res);
    return res;
}

int fclose_soloader(FILE * f) {
    f=stdio_host(f);
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int close_soloader(int fd) {
    if (sif_pack_is_fd(fd)) {
        sif_pack_close(fd);
        return 0;
    }
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    struct sif_asset_ref ref;
    const char *path = sif_io_target(_pathname, &ref);
    DIR* ret = sif_pack_opendir(&ref, path);
    if (ret == (DIR*)-1)
        ret = NULL;
    else if (!ret)
        ret = opendir(path);
    l_debug("opendir(\"%s\"): %p", _pathname, ret);
    return ret;
}

static dirent64_bionic * pack_dirent(DIR * dir, dirent64_bionic * out) {
    unsigned char type;
    const char *name = sif_pack_readdir(dir, &type);
    if (!name)
        return NULL;
    memset(out, 0, sizeof(*out));
    strncpy(out->d_name, name, sizeof(out->d_name) - 1);
    out->d_type = type;
    return out;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;
    if (sif_pack_is_dir(dir))
        return pack_dirent(dir, &dirent_tmp);

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    if (sif_pack_is_dir(dirp)) {
        *result = pack_dirent(dirp, entry);
        return 0;
    }
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    if (sif_pack_is_dir(dir)) {
        sif_pack_closedir(dir);
        return 0;
    }
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    // Android/newlib command numbers 0..4 match, but status flag bits differ.
    // Record locking and less common commands need a separate ABI bridge.
    int ret;
    if(cmd==0 || cmd==2 || cmd==4) {
        va_list a;va_start(a,cmd);int value=va_arg(a,int);va_end(a);
        if(cmd==4)value=oflags_bionic_to_newlib(value);
        ret=fcntl(fd,cmd,value);
    } else if(cmd==1 || cmd==3) {
        ret=fcntl(fd,cmd);
        if(cmd==3 && ret>=0) {
            int flags=(ret&O_ACCMODE)==O_RDWR?BIONIC_O_RDWR:(ret&O_ACCMODE)==O_WRONLY?BIONIC_O_WRONLY:0;
            if(ret&O_APPEND)flags|=BIONIC_O_APPEND;
            if(ret&O_NONBLOCK)flags|=BIONIC_O_NONBLOCK;
            ret=flags;
        }
    } else {errno=ENOSYS;ret=-1;}
    l_debug("fcntl(%i, %i): %i",fd,cmd,ret);
    return ret;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    errno=ENOSYS;
    return -1;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}

ssize_t read_soloader(int fd, void *buf, size_t n) {
    return sif_pack_is_fd(fd) ? sif_pack_read(fd, buf, n) : read(fd, buf, n);
}

ssize_t write_soloader(int fd, const void *buf, size_t n) {
    if (sif_pack_is_fd(fd)) {
        errno = EBADF;
        return -1;
    }
    return write(fd, buf, n);
}

off_t lseek_soloader(int fd, off_t offset, int whence) {
    return sif_pack_is_fd(fd) ? sif_pack_lseek(fd, offset, whence) : lseek(fd, offset, whence);
}

int ftruncate_soloader(int fd, off_t length) {
    if (sif_pack_is_fd(fd)) {
        errno = EINVAL;
        return -1;
    }
    return ftruncate(fd, length);
}

FILE * fdopen_soloader(int fd, const char * mode) {
    return sif_pack_is_fd(fd) ? sif_pack_fdopen(fd, mode) : fdopen(fd, mode);
}
