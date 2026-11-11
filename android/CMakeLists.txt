# android/CMakeLists.txt: the native part of the Android app, once per ABI.
# The Makefile runs it with the NDK toolchain (make PLATFORM=android); see
# android/README.md.
#   libSDL2.so        SDL2, from android/deps
#   libSDL2_mixer.so  SDL2_mixer, WAV only (the music goes through fdk-aac)
#   libtama.so        armeabi-v7a: loader-arm, src/arm, src/android_emu and
#                     the rest: the game's .so runs in this process.
#                     arm64-v8a, x86_64: loader, src/host and the rest: the
#                     .so runs in loader-arm, another process (see
#                     src/platform/android).
#                     Both with the sdl backend in GLES 1.1 and the android
#                     platform. SDL loads it and calls SDL_main.
cmake_minimum_required(VERSION 3.16)
project(tamatama C CXX ASM)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps/src)

# SDL2 and SDL2_mixer, shared: SDLActivity loads them by name.
add_subdirectory(${DEPS}/SDL2 SDL2)
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
foreach(codec OPUS FLAC MOD MIDI WAVPACK GME MP3 VORBIS_TREMOR VORBIS_VORBISFILE)
  set(SDL2MIXER_${codec} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL2_mixer SDL2_mixer)

# fdk-aac, static, for the .m4a music (backend/sdl/m4a.c).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/fdk-aac fdk-aac)
# On Android fdk-aac includes <log/log.h>, an AOSP header the NDK lacks.
target_include_directories(fdk-aac PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stub)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

# loader-arm or loader, as in the Makefile, without the debug console (no Unix
# sockets worth having on Android).
file(GLOB SOURCES
  ${REPO}/src/shared/*.c
  ${REPO}/src/debug/*.c
  ${REPO}/src/transfer/*.c
  ${REPO}/src/third_party/*.c
  ${REPO}/src/third_party/miniz/*.c
  ${REPO}/src/backend/*.c
  ${REPO}/src/backend/sdl/*.c
  ${REPO}/src/platform/android/*.c
  ${REPO}/src/port/*.c ${REPO}/src/port/*.cpp
  ${REPO}/src/port/*/*.c ${REPO}/src/port/*/*.cpp)
if(ANDROID_ABI STREQUAL "armeabi-v7a")
  file(GLOB PROGRAM ${REPO}/src/arm/*.c ${REPO}/src/arm/asm/*.s ${REPO}/src/android_emu/*.c)
else()
  file(GLOB PROGRAM ${REPO}/src/host/*.c)
endif()
list(APPEND SOURCES ${PROGRAM})
list(FILTER SOURCES EXCLUDE REGEX "debug_server\\.c$")
file(GLOB INCLUDE_DIRS LIST_DIRECTORIES true ${REPO}/src/* ${REPO}/src/port/*)
list(FILTER INCLUDE_DIRS EXCLUDE REGEX "\\.(c|h|md|cpp|s)$")

add_library(tama SHARED ${SOURCES})
target_include_directories(tama PRIVATE ${INCLUDE_DIRS}
  ${REPO}/src/third_party/miniz ${REPO}/src/third_party/stb ${REPO}/src/third_party/minimp4
  ${CMAKE_CURRENT_BINARY_DIR}/fdk-aac-include)
target_compile_definitions(tama PRIVATE _GNU_SOURCE LOADER_NATIVE=0 ARENA_GLES
  MINIZ_NO_TIME MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_compile_options(tama PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-O2 -ffp-contract=off -U_FORTIFY_SOURCE
  -fno-stack-protector -Wall -Wno-unused-function -Wno-attributes -Wno-pointer-to-int-cast
  -Wno-int-to-pointer-cast>)
target_compile_options(tama PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -fno-exceptions -fno-rtti
  -fno-threadsafe-statics>)
if(ANDROID_ABI STREQUAL "armeabi-v7a")
  target_compile_options(tama PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-marm>)
endif()
# miniz warns that its generic file path has no large files: the .apk files
# are far smaller than 2 GiB
set_source_files_properties(${REPO}/src/third_party/miniz/miniz.c PROPERTIES COMPILE_OPTIONS -Wno-\#pragma-messages)
# the definitions also reach the assembly files, which do not use them
target_compile_options(tama PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>)
# The program's main is arena_main (loader-arm) or host_main (loader);
# SDL_main is in platform/android.
set_source_files_properties(${REPO}/src/arm/arm_main.c PROPERTIES COMPILE_DEFINITIONS main=arena_main)
set_source_files_properties(${REPO}/src/host/host.c PROPERTIES COMPILE_DEFINITIONS main=host_main)

# fdk-aac installs its headers as <fdk-aac/...>: the same path for m4a.c.
file(GLOB FDK_HEADERS ${DEPS}/fdk-aac/libAACdec/include/*.h ${DEPS}/fdk-aac/libSYS/include/*.h)
file(COPY ${FDK_HEADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fdk-aac-include/fdk-aac)

target_link_libraries(tama PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer fdk-aac GLESv1_CM log android m dl)
