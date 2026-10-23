#pragma once
#include "asset_paths.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#ifndef DT_DIR
#define DT_REG 8
#define DT_DIR 4
#endif
/* Read-only assets.bin overlay for the install and external trees.
 * Lookups try the pack first. A path the game later writes, renames or
 * deletes is "shadowed": its pack copy is ignored from then on and the loose
 * file decides, persistently across launches. Format: tools/sifpack.py. */
#define SIF_PACK_MAGIC "SIFPACK"
#define SIF_PACK_VERSION 1
struct sif_pack_header {
    char magic[8];
    uint32_t version,header_size,entry_count,index_crc;
    uint64_t index_offset,file_size;
    uint32_t mtime,reserved[7];
};
/* Sorted by hash. Data offsets are in 16-byte units. */
struct sif_pack_entry { uint64_t hash; uint32_t offset16,size_flags; };
#define SIF_PACK_DIRECTORY 0x80000000u
_Static_assert(sizeof(struct sif_pack_header)==72,"pack header");
_Static_assert(sizeof(struct sif_pack_entry)==16,"pack entry");

struct sif_pack_stat { int directory; uint32_t size; uint32_t mtime; };
void sif_pack_init(void);
int sif_pack_available(void);
/* 1 when the pack holds the tree root (install or external). */
int sif_pack_has_root(int root);
int sif_pack_stat(const struct sif_asset_ref *ref,struct sif_pack_stat *out);

/* Wrappers used by the I/O bridge. Each returns 1 when it handled the call
 * (result stored), 0 when the caller should use the loose filesystem. */
int sif_pack_open(const struct sif_asset_ref *ref,int newlib_flags,int *fd);
int sif_pack_fopen(const struct sif_asset_ref *ref,const char *mode,FILE **file);
/* Prepares a loose copy before a mutation: copies a pack file up when the
 * mutation keeps its contents, then shadows the path. */
enum { SIF_PACK_REPLACE, SIF_PACK_MODIFY, SIF_PACK_REMOVE };
int sif_pack_before_write(const struct sif_asset_ref *ref,int kind);
/* Creates missing physical parent directories of a loose asset path. */
int sif_pack_make_parents(const char *physical);

int sif_pack_is_fd(int fd);
ssize_t sif_pack_read(int fd,void *buf,size_t n);
off_t sif_pack_lseek(int fd,off_t offset,int whence);
int sif_pack_fstat(int fd,uint32_t *size,uint32_t *mtime);
void sif_pack_close(int fd);
FILE *sif_pack_fdopen(int fd,const char *mode);

/* Directory listing merging pack entries with the loose tree. */
DIR *sif_pack_opendir(const struct sif_asset_ref *ref,const char *physical);
int sif_pack_is_dir(DIR *dir);
/* Returns the next name and type (DT_REG or DT_DIR), NULL at the end. */
const char *sif_pack_readdir(DIR *dir,unsigned char *type);
void sif_pack_closedir(DIR *dir);
