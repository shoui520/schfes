#pragma once
#include <stddef.h>
/* Every game-owned file lives below ux0:data/sif. The install and external
 * trees mirror Android's overlay layout; assets.bin may supply both. */
#define SIF_ROOT "ux0:/data/sif"
#define INSTALL_ROOT SIF_ROOT "/sif-install"
#define EXTERNAL_ROOT SIF_ROOT "/sif-external"
#define INSTALL_PATH INSTALL_ROOT "/"
#define EXTERNAL_PATH EXTERNAL_ROOT "/"
#define SIF_PACK_PATH SIF_ROOT "/assets.bin"
#define SIF_PACK_SHADOW_PATH SIF_ROOT "/assets.bin.shadow"
/* The process rejects paths with more than ten components after the mount
 * (ux0:/a/.../j). Deeper asset paths are stored with their leading components
 * joined by SIF_COLLAPSE_SEPARATOR; tools/sifpack.py writes the same layout. */
#define SIF_MAX_PATH_COMPONENTS 10
#define SIF_COLLAPSE_SEPARATOR '%'
enum { SIF_ASSET_NONE=-1,SIF_ASSET_INSTALL=0,SIF_ASSET_EXTERNAL=1 };
/* Normalized location of an asset path; rel has no leading or repeated
 * slashes, no "." components and no trailing slash. */
struct sif_asset_ref { int root; char rel[1024]; };
/* Returns the root id, or SIF_ASSET_NONE when the path is outside both trees. */
int sif_asset_split(const char *path,struct sif_asset_ref *out);
/* Physical location for a normalized reference, with deep directories
 * collapsed. Returns -1 with ENAMETOOLONG when it does not fit. */
int sif_asset_physical(const struct sif_asset_ref *ref,char *out,size_t capacity);
/* The uncollapsed spelling of a reference that sif_asset_physical collapses,
 * or NULL. Deep files copied before collapsing existed live there. */
const char *sif_asset_uncollapsed(const struct sif_asset_ref *ref);
/* Maps a game path to its physical spelling; other paths are returned as-is.
 * Results use a small thread-local ring so two operands can coexist. */
const char *sif_resolve_asset_path(const char *path);
