# NPPS4 server port. Built as a library for the Vita loader and, standalone,
# as a host executable for testing against the Python server:
#   cmake -S src/server -B build-host && cmake --build build-host
cmake_minimum_required(VERSION 3.16)
project(npps CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE NPPS_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/db/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/system/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/data/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/webview/*.cpp)

add_library(npps OBJECT ${NPPS_SOURCES})
target_include_directories(npps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(npps PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wno-deprecated-declarations>)

if(VITA)
  target_sources(npps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vita/vita_server.cpp)
  # Server data, beatmaps and the default key come from an NPPS4 checkout.
  set(NPPS4_SOURCE_DIR "" CACHE PATH "NPPS4 checkout (npps4/server_data.json, beatmaps/, default_server_key.pem)")
  if(NOT EXISTS "${NPPS4_SOURCE_DIR}/npps4/server_data.json" OR NOT IS_DIRECTORY "${NPPS4_SOURCE_DIR}/beatmaps")
    message(FATAL_ERROR "Set NPPS4_SOURCE_DIR to an NPPS4 checkout")
  endif()
  # NPPS4's default key; the game client is patched to trust it.
  set(NPPS4_SERVER_KEY "${NPPS4_SOURCE_DIR}/default_server_key.pem" CACHE FILEPATH
      "PEM private key the server signs responses with")
  if(NOT EXISTS "${NPPS4_SERVER_KEY}")
    message(FATAL_ERROR "Set NPPS4_SERVER_KEY to NPPS4's default_server_key.pem")
  endif()

  # app0:npps/: server_data.json and beatmaps.bin, one archive of the gzipped beatmaps
  # (tools/npps_beatmaps.py); loose files would install very slowly from the VPK.
  set(NPPS_PACKAGE_DIR ${CMAKE_BINARY_DIR}/npps-package CACHE INTERNAL "")
  file(REMOVE_RECURSE ${NPPS_PACKAGE_DIR}/beatmaps)
  file(MAKE_DIRECTORY ${NPPS_PACKAGE_DIR})
  configure_file(${NPPS4_SOURCE_DIR}/npps4/server_data.json ${NPPS_PACKAGE_DIR}/server_data.json COPYONLY)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/npps_beatmaps.py ${NPPS4_SOURCE_DIR}/beatmaps
            ${NPPS_PACKAGE_DIR}/beatmaps.bin
    RESULT_VARIABLE NPPS_BEATMAPS_RESULT)
  if(NOT NPPS_BEATMAPS_RESULT EQUAL 0)
    message(FATAL_ERROR "Packing ${NPPS4_SOURCE_DIR}/beatmaps failed")
  endif()
  file(READ "${NPPS4_SERVER_KEY}" NPPS4_SERVER_KEY_PEM)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/server_key.cpp
    "extern const char npps_embedded_server_key[];\nconst char npps_embedded_server_key[] = R\"PEM(${NPPS4_SERVER_KEY_PEM})PEM\";\n")
  target_sources(npps PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/server_key.cpp)
  # The loader is softfp for the Android game; the server only talks to the
  # loader through integer/pointer C calls and uses the hardfp SDK libraries.
  target_compile_options(npps PRIVATE -mfloat-abi=hard -O2 -fexceptions)
  target_include_directories(npps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  include(FetchContent)
  FetchContent_Declare(sqlite
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
    URL_HASH SHA256=77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(sqlite)
  add_library(npps_sqlite STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
  target_include_directories(npps_sqlite PUBLIC ${sqlite_SOURCE_DIR})
  target_compile_definitions(npps_sqlite PRIVATE
    SQLITE_OS_OTHER=1 SQLITE_THREADSAFE=0 SQLITE_TEMP_STORE=3 SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DQS=0 SQLITE_MAX_MMAP_SIZE=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1)
  target_compile_options(npps_sqlite PRIVATE -O2 -w -mfloat-abi=hard)
  target_link_libraries(npps PUBLIC npps_sqlite crypto z)
else()
  find_package(SQLite3 REQUIRED)
  find_package(OpenSSL REQUIRED)
  find_package(ZLIB REQUIRED)
  target_link_libraries(npps PUBLIC SQLite::SQLite3 OpenSSL::Crypto ZLIB::ZLIB)
  add_executable(npps-host host/main.cpp)
  target_link_libraries(npps-host PRIVATE npps)
  add_executable(npps-tests host/tests.cpp)
  target_link_libraries(npps-tests PRIVATE npps)
endif()
