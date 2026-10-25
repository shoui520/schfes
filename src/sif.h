#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#define BASE_PATH "ux0:/data/sif"
#define DATA_PATH "ux0:/data/sif/"
#include "asset_paths.h"
#define SIF_ANDROID_SDK 19
#define SIF_ANDROID_RELEASE "4.4"
#define SIF_DEVICE_PRODUCT "vita"
#define SIF_DEVICE_MANUFACTURER "Sony"
#define SIF_DEVICE_BRAND "Sony"
#define SIF_DEVICE_DEVICE "vita"
#define SIF_DEVICE_MODEL "PS Vita"
#define SIF_DEVICE_BOARD "vita"
// Identify this compatibility build without claiming Android signing keys.
#define SIF_BUILD_TAGS "unsigned"
#define SIF_BUILD_FINGERPRINT SIF_DEVICE_BRAND "/" SIF_DEVICE_PRODUCT "/" SIF_DEVICE_DEVICE ":" SIF_ANDROID_RELEASE "/SIFV00001/0.01:homebrew/" SIF_BUILD_TAGS
#ifndef SO_PATH
#define SO_PATH DATA_PATH "libGame.so"
#endif
extern so_module so_mod;
void sif_log(const char *fmt,...) __attribute__((format(printf,1,2)));
void sif_stop(const char *reason) __attribute__((noreturn));
void sif_write_server_info(void);
int resolve_imports(so_module *mod);
// Hooks into libGame.so; install after relocation, before flushing caches.
void sif_install_game_fixes(void);
void sif_java_init(void);
void sif_java_check_method(const char *name,const char *signature);
void sif_java_check_field(const char *name,const char *signature);
void sif_java_bind_method(jmethodID id,const char *signature);
void sif_java_pack_args(jmethodID,const jvalue*,void*,size_t,va_list*);

void sif_install_assets(void);
void sif_prepare_external_path(void);

void sif_input_init(void);
void sif_input_poll(void);
void sif_input_release(void);

void sif_java_power_tick(void);
