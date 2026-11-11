# portmaster/portmaster.mk: make PLATFORM=portmaster builds tamatama.zip, the
# port for PortMaster (handhelds with Linux). Included by the Makefile.
#   1. portmaster/deps: Debian buster for arm64 and armhf (glibc 2.28, SDL
#      2.0.9, GLES 1: old enough for every CFW), fdk-aac and qemu-arm,
#      downloaded once. No root and no container.
#   2. fdk-aac, static, for each architecture
#   3. with clang and lld, against those sysroots:
#        tamatama.aarch64  loader: src/host and the sdl backend in GLES 1.1
#        tamatama.armhf    loader-arm with the sdl backend: everything in one
#                          program, as on a 32-bit Pi
#   4. loader-arm, static (the Makefile's, for PLATFORM=linux): the game's .so
#      on aarch64, on the CPU or under qemu-arm.aarch64 (Debian's, static)
#   5. tamatama.zip: Tamatama.sh and tamatama/ (port.json, README.md,
#      gameinfo.xml, screenshot.png, cover.png, licenses/, apk/ and the
#      programs), as PortMaster expects
# The SDL2, SDL2_mixer and GLES libraries are the device's.
PM          := obj/portmaster
PM_DEPS     := portmaster/deps
PM_ARCHIVE  := http://archive.debian.org/debian
PM_SUITE    := buster
PM_PACKAGES := libc6 libc6-dev linux-libc-dev libgcc1 libgcc-8-dev libstdc++6 libstdc++-8-dev \
               libsdl2-2.0-0 libsdl2-dev libsdl2-mixer-2.0-0 libsdl2-mixer-dev libgles1 libglvnd0
PM_FDK_URL  := https://github.com/mstorsjo/fdk-aac/archive/refs/tags/v2.0.3.tar.gz
# qemu-arm, static, from Debian's qemu-user (GPL-2.0, see the QEMU notice)
PM_QEMU_VERSION := 10.0.13+ds-0+deb13u1
PM_QEMU_POOL    := https://deb.debian.org/debian/pool/main/q/qemu/
PM_VERSION  := $(shell sed -n 's/^\#define TAMATAMA_VERSION "\(.*\)"/\1/p' src/shared/common.h)
PM_ZIP      := tamatama.zip

# Debian's name for each architecture, and clang's.
DEB_aarch64    := arm64
DEB_armhf      := armhf
TARGET_aarch64 := --target=aarch64-linux-gnu
TARGET_armhf   := --target=arm-linux-gnueabihf -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
SYSROOT_aarch64 := $(CURDIR)/$(PM_DEPS)/sysroot-arm64
SYSROOT_armhf   := $(CURDIR)/$(PM_DEPS)/sysroot-armhf
PM_CC = clang --no-default-config $(TARGET_$(1)) --sysroot=$(SYSROOT_$(1))

PM_INCLUDES := $(addprefix -I,$(wildcard src/*/ src/port/*/)) -Isrc/third_party/miniz -Isrc/third_party/stb \
               -Isrc/third_party/minimp4 -Iportmaster/include
PM_CFLAGS = -O2 -ffp-contract=off -D_GNU_SOURCE -D_REENTRANT -U_FORTIFY_SOURCE -DLOADER_NATIVE=0 -DARENA_GLES \
            -DMINIZ_NO_TIME -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES $(PM_INCLUDES) -I$(SYSROOT_$(1))/usr/include/SDL2 \
            -I$(PM)/$(1)/include -Wall -Wno-unused-function
PM_CXXFLAGS := -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics
PM_ARMFLAGS := -marm -fno-pie -fno-stack-protector -Wno-attributes -Wno-pointer-to-int-cast \
               -Wno-int-to-pointer-cast
PM_LIBS = $(PM)/$(1)/libfdk-aac.a -lSDL2 -lSDL2_mixer -l:libGLESv1_CM.so.1 -lm -ldl -lpthread \
          -static-libgcc -Wl,--allow-shlib-undefined

# The same sources as the Makefile, with the linux platform and the sdl
# backend.
PM_BOTH  := $(wildcard src/shared/*.c src/debug/*.c src/transfer/*.c src/third_party/*.c src/third_party/miniz/*.c \
            src/backend/*.c src/backend/sdl/*.c src/platform/linux/*.c src/port/*.c src/port/*.cpp \
            src/port/*/*.c src/port/*/*.cpp)
SRC_aarch64 := $(PM_BOTH) $(wildcard src/host/*.c)
SRC_armhf   := $(PM_BOTH) $(wildcard src/arm/*.c src/arm/asm/*.s src/android_emu/*.c)
PM_HEADERS  := $(wildcard src/*/*.h src/backend/*/*.h src/platform/*/*.h src/port/*/*.h \
               src/third_party/*/*.h)

all: $(PM_ZIP)

# 1. A sysroot: the packages unpacked. Their absolute symlinks (libm.so ->
#    /lib/...) would point to this PC: they become relative.
$(PM_DEPS)/sysroot-%/.ready:
	@echo "Downloading Debian $(PM_SUITE) for $* into $(@D) (only the first time)"
	rm -rf $(@D).tmp && mkdir -p $(@D).tmp
	curl -fsSL $(PM_ARCHIVE)/dists/$(PM_SUITE)/main/binary-$*/Packages.xz | xz -d > $(@D).tmp/Packages
	set -e; for p in $(PM_PACKAGES); do \
	  f=$$(awk -v p=$$p '$$1=="Package:"{ok=$$2==p} ok&&$$1=="Filename:"{print $$2;exit}' $(@D).tmp/Packages); \
	  test -n "$$f" || { echo "no package $$p for $*"; exit 1; }; \
	  curl -fsSL $(PM_ARCHIVE)/$$f -o $(@D).tmp/package.deb; \
	  (cd $(@D).tmp && ar x package.deb && tar -xf data.tar.* && rm -f package.deb data.tar.* control.tar.* debian-binary); \
	done
	cd $(@D).tmp && find . -type l | while read link; do \
	  target=$$(readlink "$$link"); case "$$target" in /*) \
	    ln -sfn "$$(realpath -m --relative-to="$$(dirname "$$link")" ".$$target")" "$$link";; esac; done
	rm -f $(@D).tmp/Packages && touch $(@D).tmp/.ready && rm -rf $(@D) && mv $(@D).tmp $(@D)

$(PM_DEPS)/src/fdk-aac/.ready:
	rm -rf $(@D) && mkdir -p $(PM_DEPS)/src
	curl -fsSL $(PM_FDK_URL) | tar xz -C $(PM_DEPS)/src && mv $(PM_DEPS)/src/fdk-aac-* $(@D)
	touch $@

$(PM_DEPS)/qemu-arm.aarch64:
	rm -rf $(PM_DEPS)/qemu.tmp && mkdir -p $(PM_DEPS)/qemu.tmp
	curl -fsSL -o $(PM_DEPS)/qemu.tmp/qemu.deb $(PM_QEMU_POOL)qemu-user_$(PM_QEMU_VERSION)_arm64.deb
	cd $(PM_DEPS)/qemu.tmp && ar x qemu.deb && tar xf data.tar.xz ./usr/bin/qemu-arm
	mv $(PM_DEPS)/qemu.tmp/usr/bin/qemu-arm $@ && rm -rf $(PM_DEPS)/qemu.tmp

# 2. fdk-aac, static, and its headers as <fdk-aac/...>, as m4a.c includes them.
$(PM)/%/libfdk-aac.a: $(PM_DEPS)/src/fdk-aac/.ready | $(PM_DEPS)/sysroot-arm64/.ready $(PM_DEPS)/sysroot-armhf/.ready
	cmake -S $(PM_DEPS)/src/fdk-aac -B $(PM)/$*/fdk -G Ninja -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
	  -DCMAKE_C_FLAGS="--no-default-config $(TARGET_$*)" -DCMAKE_CXX_FLAGS="--no-default-config $(TARGET_$*)" \
	  -DCMAKE_SYSROOT=$(SYSROOT_$*) -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	  -DBUILD_SHARED_LIBS=OFF -DBUILD_PROGRAMS=OFF >/dev/null
	cmake --build $(PM)/$*/fdk
	cp $(PM)/$*/fdk/libfdk-aac.a $@
	mkdir -p $(PM)/$*/include/fdk-aac
	cp $(PM_DEPS)/src/fdk-aac/libAACdec/include/*.h $(PM_DEPS)/src/fdk-aac/libSYS/include/*.h $(PM)/$*/include/fdk-aac/

# 3. The programs. Each architecture keeps its objects in obj/portmaster/ARCH.
define PM_PROGRAM
$(PM)/$(1)/obj/%.c.o: src/%.c $(PM_HEADERS) $(PM)/$(1)/libfdk-aac.a
	@mkdir -p $$(@D) && $(call PM_CC,$(1)) $(call PM_CFLAGS,$(1)) $(2) -c $$< -o $$@
$(PM)/$(1)/obj/%.cpp.o: src/%.cpp $(PM_HEADERS) $(PM)/$(1)/libfdk-aac.a
	@mkdir -p $$(@D) && $(call PM_CC,$(1)) -x c++ $(call PM_CFLAGS,$(1)) $(PM_CXXFLAGS) $(2) -c $$< -o $$@
$(PM)/$(1)/obj/%.s.o: src/%.s $(PM)/$(1)/libfdk-aac.a
	@mkdir -p $$(@D) && $(call PM_CC,$(1)) -c $$< -o $$@
$(PM)/tamatama.$(1): $(SRC_$(1):src/%=$(PM)/$(1)/obj/%.o)
	$(call PM_CC,$(1)) -fuse-ld=lld $(3) $$^ -o $$@ $(call PM_LIBS,$(1)) -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
endef
$(eval $(call PM_PROGRAM,aarch64,,))
$(eval $(call PM_PROGRAM,armhf,$(PM_ARMFLAGS),-no-pie))

# 4. loader-arm, static, from the Makefile (the same as on the PC).
pm-loader-arm:
	$(MAKE) PLATFORM=linux loader-arm

# 5. The .zip.
PM_STAGE := $(PM)/zip
$(PM_ZIP): $(PM)/tamatama.aarch64 $(PM)/tamatama.armhf pm-loader-arm $(PM_DEPS)/qemu-arm.aarch64 \
           $(wildcard portmaster/Tamatama.sh portmaster/tamatama/*)
	rm -rf $(PM_STAGE) && mkdir -p $(PM_STAGE)/tamatama/licenses $(PM_STAGE)/tamatama/apk
	cp portmaster/Tamatama.sh $(PM_STAGE)/
	cp portmaster/tamatama/* $(PM_STAGE)/tamatama/
	cp $(PM)/tamatama.aarch64 $(PM)/tamatama.armhf loader-arm $(PM_DEPS)/qemu-arm.aarch64 $(PM_STAGE)/tamatama/
	echo "$(PM_VERSION)" > $(PM_STAGE)/tamatama/VERSION
	echo "Copy the original .apk files of the Tamagotchi games here." > $(PM_STAGE)/tamatama/apk/README.txt
	cp LICENSE $(PM_STAGE)/tamatama/licenses/tamatama.txt
	cp $(PM_DEPS)/src/fdk-aac/NOTICE $(PM_STAGE)/tamatama/licenses/fdk-aac.txt
	cp android/licenses/GPL-2.0.txt $(PM_STAGE)/tamatama/licenses/GPL-2.0.txt
	cp src/third_party/miniz/LICENSE $(PM_STAGE)/tamatama/licenses/miniz.txt
	cp src/third_party/minimp4/LICENSE $(PM_STAGE)/tamatama/licenses/minimp4.txt
	cp src/third_party/stb/LICENSE $(PM_STAGE)/tamatama/licenses/stb.txt
	printf '%s\n' \
	  "qemu-arm.aarch64 is qemu-arm (QEMU user mode emulation), unmodified, from" \
	  "the Debian package qemu-user $(PM_QEMU_VERSION)." \
	  "" \
	  "QEMU is free software: it is distributed under the GNU General Public" \
	  "License, version 2 (GPL-2.0.txt), and parts of it under compatible" \
	  "licenses. Its complete source code is in the same Debian pool:" \
	  "" \
	  "  $(PM_QEMU_POOL)" \
	  "  qemu_$(firstword $(subst -, ,$(PM_QEMU_VERSION))).orig.tar.xz" \
	  "  qemu_$(PM_QEMU_VERSION).debian.tar.xz" \
	  "  qemu_$(PM_QEMU_VERSION).dsc" \
	  "" \
	  "and at https://www.qemu.org. The same source is also available from the" \
	  "author of Tamatama on request." > $(PM_STAGE)/tamatama/licenses/QEMU.txt
	chmod +x $(PM_STAGE)/Tamatama.sh $(PM_STAGE)/tamatama/tamatama.* $(PM_STAGE)/tamatama/loader-arm \
	  $(PM_STAGE)/tamatama/qemu-arm.aarch64
	rm -f $@ && cd $(PM_STAGE) && zip -q -r -9 $(CURDIR)/$@ Tamatama.sh tamatama
	@echo "Built $@ (version $(PM_VERSION)): unzip it into the ports folder of the device"

clean: ; rm -rf $(PM) $(PM_ZIP)
distclean: clean ; rm -rf $(PM_DEPS)

.PHONY: all pm-loader-arm clean distclean
