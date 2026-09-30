#pragma once
#define INSTALL_ROOT "ux0:/data/sif/install"
#define EXTERNAL_ROOT "ux0:/data/sif/external"
#define DEV_INSTALL_ROOT "ux0:/data/sif-install"
#define DEV_EXTERNAL_ROOT "ux0:/sif-external"
#define LEGACY_EXTERNAL_ROOT "ux0:/data/sif-external"
#define LEGACY_EXTERNAL_PATH LEGACY_EXTERNAL_ROOT "/"
/* Selected once at startup; never move or merge existing trees. */
extern const char *sif_install_path;
extern const char *sif_external_path;
#define INSTALL_PATH sif_install_path
#define EXTERNAL_PATH sif_external_path
int sif_select_asset_paths(void);
const char *sif_resolve_asset_path(const char *path);
