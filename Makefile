# Makefile: "make" builds everything this machine needs.
#   32-bit ARM:    loader-arm, with the system compiler and its libc.
#   Anything else: loader and loader-arm. loader-arm is built with clang against
#                  an ARM libc, which the first run downloads from Debian into
#                  sysroot/.
# BACKEND chooses the window, input and GL code: make BACKEND=sdl (default),
# dummy.
# PLATFORM chooses the system code (folders, where the .apk is): make
# PLATFORM=linux (default).
# DEBUG_CONSOLE=0 leaves out the debug console (--debug), for systems without
# Unix sockets.
# NATIVE=1 builds the end of the migration: no .so and no QEMU, everything is
# native code.
#   The .apk is still needed, for its resources.
#   loader-arm cannot use the libraries of your PC, so with sdl on a PC it gets
#   dummy.
# The folder of each file decides which program it goes into:
#   src/host             only loader (64 bits)
#   src/arm              only loader-arm
#   src/arm/asm          only loader-arm: the parts written in assembly
#   src/android_emu      only loader-arm: the Android that the .so expects
#                        (bionic, pthreads, JVM)
#   src/shared           both programs
#   src/debug            both programs: watches, and the debug console unless
#                        DEBUG_CONSOLE=0
#   src/third_party      both programs: code by others, copied as is with its
#                        license
#   src/third_party/miniz  both programs: miniz, to read the .apk (MIT, see its
#                          LICENSE)
#   src/third_party/stb    both programs: stb_image, to decode the PNG textures
#                          (MIT or public domain, see its LICENSE)
#   src/backend          both programs: common to every backend
#   src/backend/BACKEND  the chosen backend
#   src/platform           both programs: platform.h, what every platform must
#                          offer
#   src/platform/PLATFORM  both programs: the chosen platform
#   src/port             both programs: your code. It may stay empty.
#   src/port/game          both programs: the game in plain C, with no ARM (only
#                          services.h, platform.h and backend.h)
#   src/port/natives       both programs: natives that read the ARM registers
#                          and call the game
BACKEND ?= sdl
PLATFORM ?= linux
DEBUG_CONSOLE ?= 1
NATIVE ?= 0
ifneq (,$(filter arm%,$(shell uname -m)))
TARGETS     := loader-arm
ARM_BACKEND := $(BACKEND)
ARM_CC      ?= cc
ARM_CXX     ?= c++
else
TARGETS     := loader loader-arm
ARM_BACKEND := $(if $(filter sdl,$(BACKEND)),dummy,$(BACKEND))
ifeq ($(origin ARM_CC),undefined)
SYSROOT    := $(CURDIR)/sysroot
READY      := $(SYSROOT)/.ready
ARM_TARGET := --no-default-config --target=arm-linux-gnueabihf --sysroot=$(SYSROOT) -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
ARM_CC     := clang $(ARM_TARGET)
ARM_CXX    := clang++ $(ARM_TARGET)
ARM_LD     := -fuse-ld=lld
STATIC     := 1
endif
endif
# A BACKEND or PLATFORM without its folder would only fail when linking, with an
# unclear message. clean and distclean do not need them.
ifeq (,$(filter clean distclean,$(MAKECMDGOALS)))
$(foreach b,$(sort $(BACKEND) $(ARM_BACKEND)),$(if $(wildcard src/backend/$(b)/*.c),,$(error no such backend: src/backend/$(b). Available: $(patsubst src/backend/%/,%,$(wildcard src/backend/*/)))))
$(if $(filter 0 1,$(DEBUG_CONSOLE)),,$(error DEBUG_CONSOLE must be 0 or 1, not $(DEBUG_CONSOLE)))
$(if $(filter 0 1,$(NATIVE)),,$(error NATIVE must be 0 or 1, not $(NATIVE)))
$(if $(wildcard src/platform/$(PLATFORM)/*.c),,$(error no such platform: src/platform/$(PLATFORM). Available: $(patsubst src/platform/%/,%,$(wildcard src/platform/*/))))
# src/port/game must stay plain C with no ARM: only services.h, platform.h,
# backend.h and its own headers.
GAME_FILES    := $(wildcard src/port/game/*.c src/port/game/*.cpp src/port/game/*.h)
GAME_INCLUDES := $(if $(GAME_FILES),$(sort $(shell sed -n 's/^[ \t]*\#[ \t]*include[ \t]*"\([^"]*\)".*/\1/p' $(GAME_FILES))))
GAME_BAD      := $(filter-out services.h platform.h backend.h $(notdir $(wildcard src/port/game/*.h)),$(GAME_INCLUDES))
$(if $(GAME_BAD),$(error src/port/game may only include services.h, platform.h, backend.h and its own headers, not: $(GAME_BAD)))
endif
INSTALL   := sudo dnf install make gcc gcc-c++ clang lld qemu-user curl xz binutils tar SDL2-devel SDL2_mixer-devel fdk-aac-free-devel mesa-libGL-devel
ifeq ($(BACKEND),sdl)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null) -lSDL2_mixer -lfdk-aac -lGL
CHECK_SDL  := check-sdl
endif
INCLUDES  := $(addprefix -I,$(wildcard src/*/ src/port/*/)) -Isrc/third_party/miniz -Isrc/third_party/stb -Isrc/third_party/minimp4
# miniz only reads the .apk: no compression, no zip writing, no file times, no
# zlib names.
MINIZ     := -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_TIME -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
CFLAGS    := -O2 -ffp-contract=off -D_GNU_SOURCE -U_FORTIFY_SOURCE -DLOADER_NATIVE=$(NATIVE) $(INCLUDES) $(MINIZ) -Wall -Wno-unused-function
CXXFLAGS  := -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics
ARM_FLAGS := -marm -fno-pie -fno-stack-protector -Wno-attributes -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
ARM_LIBS  := -lm -ldl -lpthread
ifeq ($(ARM_BACKEND),sdl)
ARM_FLAGS += $(SDL_CFLAGS)
ARM_LIBS  += $(SDL_LIBS)
endif
BOTH    := $(filter-out $(if $(filter 0,$(DEBUG_CONSOLE)),src/debug/debug_server.c),$(wildcard src/shared/*.c src/debug/*.c src/third_party/*.c src/third_party/miniz/*.c src/backend/*.c src/platform/$(PLATFORM)/*.c src/port/*.c src/port/*.cpp src/port/*/*.c src/port/*/*.cpp))
HOST    := $(BOTH) $(wildcard src/host/*.c src/backend/$(BACKEND)/*.c)
ARM     := $(BOTH) $(wildcard src/arm/*.c src/arm/asm/*.s src/android_emu/*.c src/backend/$(ARM_BACKEND)/*.c)
HEADERS := $(wildcard src/*/*.h src/backend/*/*.h src/platform/*/*.h src/port/*/*.h src/third_party/miniz/*.h src/third_party/stb/*.h src/third_party/minimp4/*.h)
STAMP   := obj/backend-$(BACKEND)-$(PLATFORM)-console$(DEBUG_CONSOLE).stamp
all: $(TARGETS)
loader: $(HOST:src/%=obj/host/%.o) $(STAMP) ; $(CC) $(filter %.o,$^) -o $@ $(SDL_LIBS)
loader-arm: $(ARM:src/%=obj/arm/%.o) $(STAMP) ; $(ARM_CC) $(ARM_LD) $(if $(STATIC),-static,-no-pie) $(filter %.o,$^) -o $@ $(ARM_LIBS)
# Changing BACKEND, PLATFORM or DEBUG_CONSOLE must link both programs again.
$(STAMP): ; @mkdir -p obj && rm -f obj/backend-*.stamp && touch $@
# Changing NATIVE changes the code itself: everything is compiled again.
NATIVE_STAMP := obj/native-$(NATIVE).stamp
$(NATIVE_STAMP): ; @mkdir -p obj && rm -f obj/native-*.stamp && touch $@
obj/host/%.c.o: src/%.c $(HEADERS) $(NATIVE_STAMP) | $(CHECK_SDL) ; @mkdir -p $(@D) && $(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@
obj/host/%.cpp.o: src/%.cpp $(HEADERS) $(NATIVE_STAMP) | $(CHECK_SDL) ; @mkdir -p $(@D) && $(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@
obj/arm/%.c.o: src/%.c $(HEADERS) $(NATIVE_STAMP) | $(READY) ; @mkdir -p $(@D) && $(ARM_CC) $(CFLAGS) $(ARM_FLAGS) -c $< -o $@
obj/arm/%.cpp.o: src/%.cpp $(HEADERS) $(NATIVE_STAMP) | $(READY) ; @mkdir -p $(@D) && $(ARM_CXX) $(CFLAGS) $(CXXFLAGS) $(ARM_FLAGS) -c $< -o $@
obj/arm/%.s.o: src/%.s | $(READY) ; @mkdir -p $(@D) && $(ARM_CC) -c $< -o $@
# The ARM libc: three Debian packages unpacked into sysroot/. It needs no root
# and no container.
DEBIAN   := https://deb.debian.org/debian
SUITE    := bookworm
PACKAGES := libc6-dev linux-libc-dev libgcc-12-dev
TOOLS    := $(CC) $(CXX) clang clang++ ld.lld curl xz ar tar $(if $(QEMU_ARM),,qemu-arm)
check-sdl: ; @command -v sdl2-config >/dev/null && printf '#include <SDL_mixer.h>\n#include <fdk-aac/aacdecoder_lib.h>\n' | $(CC) $(SDL_CFLAGS) -E - >/dev/null 2>&1 || { echo "Missing SDL2, SDL2_mixer or fdk-aac. Run: $(INSTALL)"; echo "Or build without a window: make BACKEND=dummy"; exit 1; }
check-tools: ; @for t in $(TOOLS); do command -v $$t >/dev/null || { echo "Missing $$t. Run: $(INSTALL)"; exit 1; }; done
$(READY): | check-tools ; @echo "Downloading an ARM libc into sysroot/ (only the first time)" && \
  rm -rf $(SYSROOT).tmp && mkdir -p $(SYSROOT).tmp && \
  curl -fsSL $(DEBIAN)/dists/$(SUITE)/main/binary-armhf/Packages.xz | xz -d > $(SYSROOT).tmp/Packages && \
  for p in $(PACKAGES); do \
    f=$$(awk -v p=$$p '$$1=="Package:"{ok=$$2==p} ok&&$$1=="Filename:"{print $$2;exit}' $(SYSROOT).tmp/Packages); \
    [ -n "$$f" ] || { echo "$$p is not in the Debian index"; exit 1; }; \
    echo "  $$f"; \
    curl -fsSL $(DEBIAN)/$$f -o $(SYSROOT).tmp/package.deb || exit 1; \
    (cd $(SYSROOT).tmp && ar x package.deb data.tar.xz && tar -xJf data.tar.xz && rm -f package.deb data.tar.xz) || exit 1; \
  done && \
  rm -f $(SYSROOT).tmp/Packages && touch $(SYSROOT).tmp/.ready && rm -rf $(SYSROOT) && mv $(SYSROOT).tmp $(SYSROOT)
clean: ; rm -rf obj loader loader-arm sysroot
distclean: clean ; rm -rf sysroot sysroot.tmp
.PHONY: all clean distclean check-tools check-sdl
