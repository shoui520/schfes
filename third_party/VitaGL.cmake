# Pinned VitaGL source; strict draw layout is required for SIF's separate color array.
file(GLOB VGL_SOURCES CONFIGURE_DEPENDS ${vitagl_SOURCE_DIR}/source/*.c ${vitagl_SOURCE_DIR}/source/*.cpp ${vitagl_SOURCE_DIR}/source/utils/*.c ${vitagl_SOURCE_DIR}/source/utils/*.cpp ${vitagl_SOURCE_DIR}/source/utils/preprocessor/*.c ${vitagl_SOURCE_DIR}/source/utils/preprocessor/*.cpp)
add_library(sif_vitagl STATIC ${VGL_SOURCES})
target_include_directories(sif_vitagl PUBLIC ${vitagl_SOURCE_DIR}/source)
target_compile_definitions(sif_vitagl PRIVATE STRICT_DRAW_COMPLIANCE VGL_GIT_HASH="464876a")
target_compile_options(sif_vitagl PRIVATE -mfloat-abi=hard -O3 -ffast-math -mtune=cortex-a9 -mfpu=neon -mfp16-format=ieee -Wno-incompatible-pointer-types -Wno-stringop-overflow "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=" "-ffile-prefix-map=${VITASDK}/=VitaSDK/")
set_target_properties(sif_vitagl PROPERTIES CXX_STANDARD 11)
