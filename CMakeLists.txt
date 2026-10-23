cmake_minimum_required(VERSION 3.18)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE FILEPATH "Vita toolchain")
if(POLICY CMP0135)
 cmake_policy(SET CMP0135 NEW)
endif()
project(sif_vita C CXX)
option(SIF_VERBOSE_COMPAT_LOG "Log each routine compatibility call" OFF)
option(SIF_STALL_DIAGNOSTICS "Log frame stalls and capture a core for explicit compatibility stops" OFF)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -O1 -g3 -std=gnu11 -D_GNU_SOURCE -D__POSIX_VISIBLE=999999 -Wno-incompatible-pointer-types")
# Host SDK archives are hardfp. Only integer/pointer calls cross directly;
# floating parameters/returns enter explicit pcs(aapcs) wrappers in these units.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-mismatch")
include(third_party/Dependencies.cmake)
add_subdirectory(src/server)
add_executable(sif_vita
 src/preferences.c src/server_info.c src/thread_affinity.c src/poll_bridge.c src/net_bridge.c src/crypto.c src/stdio_bridge.c src/input.c src/fonts.c src/assets.c src/asset_paths.c src/asset_pack.c src/compat.c src/main.c src/dynlib.c src/java.c src/java_platform.c src/java_overlay.c src/gl_bridge.c src/math_bridge.c
 src/utils/logger.c src/utils/utils.c
 src/reimpl/bits/_ctype.c src/reimpl/errno.c src/reimpl/io.c
 src/reimpl/log.c src/reimpl/mem.c src/reimpl/pthr.c src/reimpl/sys.c src/reimpl/time64.c
 ${falso_jni_SOURCE_DIR}/FalsoJNI.c ${falso_jni_SOURCE_DIR}/FalsoJNI_ImplBridge.c
 ${falso_jni_SOURCE_DIR}/FalsoJNI_Logger.c ${falso_jni_SOURCE_DIR}/converter.c
 ${sif_boilerplate_SOURCE_DIR}/lib/sha1/sha1.c ${so_util_SOURCE_DIR}/so_util.c)
set(SIF_DIAGNOSTIC_SOURCE "" CACHE FILEPATH "Optional private device integration test source")
if(SIF_DIAGNOSTIC_SOURCE)
 target_sources(sif_vita PRIVATE "${SIF_DIAGNOSTIC_SOURCE}")
 target_compile_definitions(sif_vita PRIVATE SIF_PRIVATE_DIAGNOSTICS)
endif()
set_source_files_properties(src/gl_bridge.c src/math_bridge.c src/java_overlay.c PROPERTIES COMPILE_OPTIONS "-mfloat-abi=hard")
target_include_directories(sif_vita PRIVATE src ${SIF_DEPENDENCY_INCLUDE_DIRS} ${VITASDK}/arm-vita-eabi/include/freetype2)
target_compile_options(sif_vita PRIVATE "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=" "-ffile-prefix-map=${VITASDK}/=VitaSDK/")
target_compile_definitions(sif_vita PRIVATE DEBUG_SOLOADER FALSOJNI_DEBUGLEVEL=1 USE_GLSL_SHADERS DATA_PATH="ux0:/data/sif/" SO_PATH="ux0:/data/sif/libGame.so")
if(SIF_VERBOSE_COMPAT_LOG)
 target_compile_definitions(sif_vita PRIVATE SIF_VERBOSE_COMPAT_LOG)
endif()
if(SIF_STALL_DIAGNOSTICS)
 target_compile_definitions(sif_vita PRIVATE SIF_STALL_DIAGNOSTICS)
endif()
target_link_libraries(sif_vita -Wl,--wrap=sceKernelCreateThread npps
 -Wl,--whole-archive pthread -Wl,--no-whole-archive
 sif_opensles speexdsp sndfile mp3lame mpg123 opus vorbis ogg vorbisenc vorbisfile FLAC
 sif_vitagl vitashark SceShaccCgExt taihen_stub SceShaccCg_stub
 stdc++ freetype png16 minizip ppmd bz2 lzma zstd crypto z mathneon m kubridge_stub ScePgf_stub SceRtc_stub SceHid_stub SceTouch_stub SceMotion_stub
 SceIme_stub SceSysmodule_stub SceAppMgr_stub SceAppUtil_stub SceAudio_stub SceAudioIn_stub SceCommonDialog_stub SceDisplay_stub
 SceKernelDmacMgr_stub SceCtrl_stub SceGxm_stub ScePower_stub SceVshBridge_stub SceNet_stub SceNetCtl_stub)
vita_create_self(eboot.bin sif_vita UNSAFE NOASLR)
set(SIF_PACKAGE_ASSETS "" CACHE PATH "Directory containing locally supplied LiveArea artwork")
if(NOT EXISTS "${SIF_PACKAGE_ASSETS}/livearea/template.xml")
 message(FATAL_ERROR "Set SIF_PACKAGE_ASSETS to the local artwork directory containing livearea/template.xml")
endif()
vita_create_vpk(sif_vita.vpk SIFV00001 eboot.bin VERSION 00.01 NAME "スクフェス"
 FILE ${SIF_PACKAGE_ASSETS}/livearea/icon0.png sce_sys/icon0.png
 ${SIF_PACKAGE_ASSETS}/livearea/startup.png sce_sys/livearea/contents/startup.png
 ${SIF_PACKAGE_ASSETS}/livearea/bg0.png sce_sys/livearea/contents/bg0.png
 ${SIF_PACKAGE_ASSETS}/livearea/template.xml sce_sys/livearea/contents/template.xml
 ${NPPS_PACKAGE_DIR} npps)
