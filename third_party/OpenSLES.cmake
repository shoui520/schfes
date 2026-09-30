add_library(sif_opensles STATIC
 ${opensles_SOURCE_DIR}/libopensles/VitaPcm.c
 ${opensles_SOURCE_DIR}/libopensles/OpenSLESUT.c
 ${opensles_SOURCE_DIR}/libopensles/MPH_to.c
 ${opensles_SOURCE_DIR}/libopensles/OpenSLES_IID.c
 ${opensles_SOURCE_DIR}/libopensles/classes.c
 ${opensles_SOURCE_DIR}/libopensles/devices.c
 ${opensles_SOURCE_DIR}/libopensles/trace.c
 ${opensles_SOURCE_DIR}/libopensles/locks.c
 ${opensles_SOURCE_DIR}/libopensles/sles.c
 ${opensles_SOURCE_DIR}/libopensles/sllog.c
 ${opensles_SOURCE_DIR}/libopensles/SndFile.c
 ${opensles_SOURCE_DIR}/libopensles/Vita.c
 ${opensles_SOURCE_DIR}/libopensles/IOutputMix.c
 ${opensles_SOURCE_DIR}/libopensles/IOutputMixExt.c
 ${opensles_SOURCE_DIR}/libopensles/sync.c
 ${opensles_SOURCE_DIR}/libopensles/IID_to_MPH.c
 ${opensles_SOURCE_DIR}/libopensles/ThreadPool.c
 ${opensles_SOURCE_DIR}/libopensles/C3DGroup.c
 ${opensles_SOURCE_DIR}/libopensles/CAudioPlayer.c
 ${opensles_SOURCE_DIR}/libopensles/CAudioRecorder.c
 ${opensles_SOURCE_DIR}/libopensles/CEngine.c
 ${opensles_SOURCE_DIR}/libopensles/COutputMix.c
 ${opensles_SOURCE_DIR}/libopensles/IBassBoost.c
 ${opensles_SOURCE_DIR}/libopensles/IBufferQueue.c
 ${opensles_SOURCE_DIR}/libopensles/IEffectSend.c
 ${opensles_SOURCE_DIR}/libopensles/IEngine.c
 ${opensles_SOURCE_DIR}/libopensles/IDynamicInterfaceManagement.c
 ${opensles_SOURCE_DIR}/libopensles/IEnvironmentalReverb.c
 ${opensles_SOURCE_DIR}/libopensles/IEqualizer.c
 ${opensles_SOURCE_DIR}/libopensles/IMuteSolo.c
 ${opensles_SOURCE_DIR}/libopensles/IObject.c
 ${opensles_SOURCE_DIR}/libopensles/IPlay.c
 ${opensles_SOURCE_DIR}/libopensles/IPlaybackRate.c
 ${opensles_SOURCE_DIR}/libopensles/IPrefetchStatus.c
 ${opensles_SOURCE_DIR}/libopensles/IPresetReverb.c
 ${opensles_SOURCE_DIR}/libopensles/IRecord.c
 ${opensles_SOURCE_DIR}/libopensles/ISeek.c
 ${opensles_SOURCE_DIR}/libopensles/IVirtualizer.c
 ${opensles_SOURCE_DIR}/libopensles/IVolume.c
)
target_include_directories(sif_opensles PRIVATE ${opensles_SOURCE_DIR}/include ${opensles_SOURCE_DIR}/libopensles)
target_compile_definitions(sif_opensles PRIVATE USE_OUTPUTMIXEXT USE_SDL USE_SNDFILE HAVE_PTHREAD)
target_compile_options(sif_opensles PRIVATE -mfloat-abi=hard -std=gnu17 "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=" "-ffile-prefix-map=${VITASDK}/=VitaSDK/")
