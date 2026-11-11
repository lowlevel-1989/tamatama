# android/android.mk: make PLATFORM=android builds tamatama.apk, the Android
# app. Included by the Makefile.
#   1. android/deps: SDL2, SDL2_mixer and fdk-aac sources, and Debian's static
#      qemu-arm, downloaded once
#   2. CMake + NDK, for each ABI: libSDL2.so, libSDL2_mixer.so and libtama.so
#      (GLES 1.1, platform android): android/CMakeLists.txt.
#        armeabi-v7a          libtama.so is loader-arm: phones with 32 bits
#        arm64-v8a, x86_64    libtama.so is loader, plus libloader-arm.so (the
#                             PC's static loader-arm) and libqemu-arm.so, for
#                             phones without 32 bits
#   3. javac + d8: SDL's Java code and android/java into classes.dex
#   4. aapt2: the manifest and android/res into the .apk
#   5. zipalign + apksigner, with the usual debug key (~/.android/debug.keystore)
# Needs the Android SDK with an NDK, build-tools and a platform (ANDROID_HOME,
# ~/Android/Sdk by default) and a JDK. No Gradle.
ANDROID_HOME ?= $(HOME)/Android/Sdk
NDK          ?= $(lastword $(sort $(wildcard $(ANDROID_HOME)/ndk/*)))
BUILD_TOOLS  ?= $(lastword $(sort $(wildcard $(ANDROID_HOME)/build-tools/*)))
ANDROID_JAR  ?= $(lastword $(sort $(wildcard $(ANDROID_HOME)/platforms/android-*/android.jar)))
MIN_SDK      := 21
TARGET_SDK   := 34
APP_VERSION  := $(shell sed -n 's/^\#define TAMATAMA_VERSION "\(.*\)"/\1/p' src/shared/common.h)
OUT          := obj/android
APK          := tamatama.apk
KEYSTORE     := $(HOME)/.android/debug.keystore

SDL2_URL  := https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
MIXER_URL := https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.1/SDL2_mixer-2.8.1.tar.gz
FDK_URL   := https://github.com/mstorsjo/fdk-aac/archive/refs/tags/v2.0.3.tar.gz
# qemu-arm, static, from Debian's qemu-user (GPL-2.0: its source is in the
# same pool, qemu_$(QEMU_VERSION).debian.tar.xz and qemu_*.orig.tar.xz)
QEMU_VERSION := 10.0.13+ds-0+deb13u1
QEMU_URL     := https://deb.debian.org/debian/pool/main/q/qemu/qemu-user_$(QEMU_VERSION)
DEPS      := android/deps/src
ABIS      := armeabi-v7a arm64-v8a x86_64
QEMU_ARCH_arm64-v8a := arm64
QEMU_ARCH_x86_64    := amd64

all: $(APK)

# 1. The sources, unpacked as android/deps/src/{SDL2,SDL2_mixer,fdk-aac}.
$(DEPS)/.ready:
	@echo "Downloading SDL2, SDL2_mixer and fdk-aac into android/deps/ (only the first time)"
	rm -rf $(DEPS) && mkdir -p $(DEPS)
	curl -fsSL $(SDL2_URL) | tar xz -C $(DEPS) && mv $(DEPS)/SDL2-* $(DEPS)/SDL2
	curl -fsSL $(MIXER_URL) | tar xz -C $(DEPS) && mv $(DEPS)/SDL2_mixer-* $(DEPS)/SDL2_mixer
	curl -fsSL $(FDK_URL) | tar xz -C $(DEPS) && mv $(DEPS)/fdk-aac-* $(DEPS)/fdk-aac
	touch $@

# qemu-arm for each 64-bit ABI, out of the .deb (an ar archive).
android/deps/qemu/%/qemu-arm:
	rm -rf $(@D) && mkdir -p $(@D)
	curl -fsSL -o $(@D)/qemu.deb $(QEMU_URL)_$(QEMU_ARCH_$*).deb
	cd $(@D) && ar x qemu.deb && tar xf data.tar.xz ./usr/bin/qemu-arm && mv usr/bin/qemu-arm . \
	  && rm -rf usr qemu.deb data.tar.* control.tar.* debian-binary

# 2. The native libraries, per ABI. CMake decides what to rebuild.
android-native: $(DEPS)/.ready $(foreach a,$(filter-out armeabi-v7a,$(ABIS)),android/deps/qemu/$(a)/qemu-arm)
	@test -d "$(NDK)" || { echo "No NDK in $(ANDROID_HOME)/ndk: install one with sdkmanager"; exit 1; }
	$(MAKE) PLATFORM=linux loader-arm
	rm -rf $(OUT)/apk/lib
	set -e; for abi in $(ABIS); do \
	  cmake -S android -B $(OUT)/native-$$abi -G Ninja -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_TOOLCHAIN_FILE=$(NDK)/build/cmake/android.toolchain.cmake \
	    -DANDROID_ABI=$$abi -DANDROID_PLATFORM=android-$(MIN_SDK) >/dev/null; \
	  cmake --build $(OUT)/native-$$abi; \
	  mkdir -p $(OUT)/apk/lib/$$abi; \
	  cp $(OUT)/native-$$abi/libtama.so $(OUT)/native-$$abi/SDL2/libSDL2.so \
	    $(OUT)/native-$$abi/SDL2_mixer/libSDL2_mixer.so $(OUT)/apk/lib/$$abi/; \
	  if [ $$abi != armeabi-v7a ]; then \
	    cp loader-arm $(OUT)/apk/lib/$$abi/libloader-arm.so; \
	    cp android/deps/qemu/$$abi/qemu-arm $(OUT)/apk/lib/$$abi/libqemu-arm.so; \
	  fi; \
	done

# The licenses of everything the .apk carries, as assets/licenses/*.txt: the
# start screen shows them (MainActivity, "Licenses").
QEMU_SOURCE := https://deb.debian.org/debian/pool/main/q/qemu/
android-licenses: $(DEPS)/.ready
	rm -rf $(OUT)/assets && mkdir -p $(OUT)/assets/licenses
	cp LICENSE $(OUT)/assets/licenses/tamatama.txt
	cp $(DEPS)/SDL2/LICENSE.txt $(OUT)/assets/licenses/SDL2.txt
	cp $(DEPS)/SDL2_mixer/LICENSE.txt $(OUT)/assets/licenses/SDL2_mixer.txt
	cp $(DEPS)/fdk-aac/NOTICE $(OUT)/assets/licenses/fdk-aac.txt
	cp android/licenses/GPL-2.0.txt $(OUT)/assets/licenses/GPL-2.0.txt
	cp src/third_party/miniz/LICENSE $(OUT)/assets/licenses/miniz.txt
	cp src/third_party/minimp4/LICENSE $(OUT)/assets/licenses/minimp4.txt
	cp src/third_party/stb/LICENSE $(OUT)/assets/licenses/stb.txt
	printf '%s\n' \
	  "libqemu-arm.so is qemu-arm (QEMU user mode emulation), unmodified, from" \
	  "the Debian package qemu-user $(QEMU_VERSION)." \
	  "" \
	  "QEMU is free software: it is distributed under the GNU General Public" \
	  "License, version 2 (GPL-2.0.txt), and parts of it under compatible" \
	  "licenses. Its complete source code is in the same Debian pool:" \
	  "" \
	  "  $(QEMU_SOURCE)" \
	  "  qemu_$(firstword $(subst -, ,$(QEMU_VERSION))).orig.tar.xz" \
	  "  qemu_$(QEMU_VERSION).debian.tar.xz" \
	  "  qemu_$(QEMU_VERSION).dsc" \
	  "" \
	  "and at https://www.qemu.org. The same source is also available from the" \
	  "author of Tamatama on request." > $(OUT)/assets/licenses/QEMU.txt

# 3 and 4. Resources first: aapt2 writes R.java, which the Java code uses.
JAVA := $(wildcard $(DEPS)/SDL2/android-project/app/src/main/java/org/libsdl/app/*.java) \
        $(wildcard android/java/*/*/*/*.java)
android-java: $(DEPS)/.ready android-licenses
	@test -f "$(ANDROID_JAR)" || { echo "No platform in $(ANDROID_HOME)/platforms: install one with sdkmanager"; exit 1; }
	rm -rf $(OUT)/res $(OUT)/gen $(OUT)/classes $(OUT)/dex && mkdir -p $(OUT)/res $(OUT)/gen $(OUT)/classes $(OUT)/dex
	$(BUILD_TOOLS)/aapt2 compile --dir android/res -o $(OUT)/res/res.zip
	$(BUILD_TOOLS)/aapt2 link -o $(OUT)/base.apk -I $(ANDROID_JAR) --manifest android/AndroidManifest.xml \
	  --min-sdk-version $(MIN_SDK) --target-sdk-version $(TARGET_SDK) \
	  --version-name $(APP_VERSION) --version-code 1 --java $(OUT)/gen -A $(OUT)/assets $(OUT)/res/res.zip
	javac -nowarn --release 11 -encoding UTF-8 -cp $(ANDROID_JAR) -d $(OUT)/classes \
	  $(JAVA) $$(find $(OUT)/gen -name '*.java')
	$(BUILD_TOOLS)/d8 --release --lib $(ANDROID_JAR) --min-api $(MIN_SDK) --output $(OUT)/dex \
	  $$(find $(OUT)/classes -name '*.class')

# 5. Everything into the .apk, aligned and signed.
$(APK): android-native android-java
	@test -f $(KEYSTORE) || { mkdir -p $(dir $(KEYSTORE)) && keytool -genkeypair -keystore $(KEYSTORE) \
	  -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
	  -dname "CN=Android Debug,O=Android,C=US" >/dev/null; }
	cp $(OUT)/base.apk $(OUT)/unsigned.apk
	cd $(OUT)/dex && zip -q -X ../unsigned.apk classes.dex
	cd $(OUT)/apk && zip -q -X -r ../unsigned.apk lib
	$(BUILD_TOOLS)/zipalign -p -f 4 $(OUT)/unsigned.apk $(OUT)/aligned.apk
	$(BUILD_TOOLS)/apksigner sign --ks $(KEYSTORE) --ks-pass pass:android --out $@ $(OUT)/aligned.apk
	@echo "Built $@ (version $(APP_VERSION)). Install it with: adb install -r $@"

clean: ; rm -rf $(OUT) $(APK) $(APK).idsig
distclean: clean ; rm -rf android/deps

.PHONY: all android-native android-java android-licenses clean distclean
