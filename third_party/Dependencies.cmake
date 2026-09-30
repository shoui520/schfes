# Upstream archives are pinned by full commit and verified by SHA-256.
include(FetchContent)
find_package(Git REQUIRED)

# https://github.com/v-atamanenko/FalsoJNI @ 083d5a07e025c6dfbb54ac68fd1acf696c23a86c
FetchContent_Declare(falso_jni
 URL "https://codeload.github.com/v-atamanenko/FalsoJNI/tar.gz/083d5a07e025c6dfbb54ac68fd1acf696c23a86c"
 URL_HASH SHA256=0acf6754080a832df8a525798c65cbdf591cc275b05a7bfd59c1d5c537d97b42
 SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/falso_jni"
 SOURCE_SUBDIR _sif_no_upstream_build)

# https://github.com/Rinnegatamante/so_util @ c4732373e33d808cd885dec3c2c75302cc4c739b
FetchContent_Declare(so_util
 URL "https://codeload.github.com/Rinnegatamante/so_util/tar.gz/c4732373e33d808cd885dec3c2c75302cc4c739b"
 URL_HASH SHA256=9486d8624797fc4be88384911bfa03bd5d7b08fde9ce9f52cfd0c4af60466ba1
 SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/so_util"
 SOURCE_SUBDIR _sif_no_upstream_build)

# https://github.com/frangarcj/opensles @ e35b0630ac4c9091db63374d5c111d719887dde9
FetchContent_Declare(opensles
 URL "https://codeload.github.com/frangarcj/opensles/tar.gz/e35b0630ac4c9091db63374d5c111d719887dde9"
 URL_HASH SHA256=b63358598d655d07308925418753a6615b98e264b8722545fedfa5f5f2862027
 SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/opensles"
 SOURCE_SUBDIR _sif_no_upstream_build)

# https://github.com/Rinnegatamante/vitaGL @ 464876a79cdd00650bb0eb9629b85d81f9908fd8
FetchContent_Declare(vitagl
 URL "https://codeload.github.com/Rinnegatamante/vitaGL/tar.gz/464876a79cdd00650bb0eb9629b85d81f9908fd8"
 URL_HASH SHA256=767aea418b4b098bd62dc797db75759463081dffefc6d9fa855c09367b6bdf3e
 SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/vitagl"
 SOURCE_SUBDIR _sif_no_upstream_build)

# https://github.com/v-atamanenko/soloader-boilerplate @ 0a382f0ebce348c8cdc68f32fb5fa3186baddc38
FetchContent_Declare(sif_boilerplate
 URL "https://codeload.github.com/v-atamanenko/soloader-boilerplate/tar.gz/0a382f0ebce348c8cdc68f32fb5fa3186baddc38"
 URL_HASH SHA256=0c9140ff52f1b8576067508bdd139ab996d63923a527d666314c93b65762fa81
 SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/sif_boilerplate"
 SOURCE_SUBDIR _sif_no_upstream_build)

FetchContent_MakeAvailable(falso_jni so_util opensles vitagl sif_boilerplate)

foreach(dependency falso_jni so_util opensles vitagl)
 execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
  "-DSOURCE_DIR=${${dependency}_SOURCE_DIR}"
  "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/patches/${dependency}.patch"
  -P "${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake"
  RESULT_VARIABLE patch_result)
 if(NOT patch_result EQUAL 0)
  message(FATAL_ERROR "Failed to prepare ${dependency}")
 endif()
endforeach()
set(SIF_DEPENDENCY_INCLUDE_DIRS "${FETCHCONTENT_BASE_DIR}"
 "${sif_boilerplate_SOURCE_DIR}/lib" "${opensles_SOURCE_DIR}/include")
include("${CMAKE_CURRENT_LIST_DIR}/OpenSLES.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/VitaGL.cmake")
# Record resolved locations for diagnostics and reproducible source snapshots.
file(WRITE "${CMAKE_BINARY_DIR}/sif-dependencies.json"
 "{\n  \"falso_jni\": \"${falso_jni_SOURCE_DIR}\",\n  \"so_util\": \"${so_util_SOURCE_DIR}\",\n  \"opensles\": \"${opensles_SOURCE_DIR}\",\n  \"vitaGL\": \"${vitagl_SOURCE_DIR}\",\n  \"soloader-boilerplate\": \"${sif_boilerplate_SOURCE_DIR}\"\n}\n")
