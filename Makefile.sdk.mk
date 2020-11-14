include config.mk
include releng/deps.mk


MAKE_J ?= -j 8
SHELL := /bin/bash


ifeq ($(glib_iconv_option), -Diconv=external)
	iconv := build/fs-%/lib/libiconv.a
endif

ifeq ($(host_os), $(filter $(host_os), linux android qnx))
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	elf := build/fs-%/lib/libelf.a
	dwarf := build/fs-%/lib/libdwarf.a
endif

ifeq ($(host_os), $(filter $(host_os), macos ios linux android))
	glib_tls_provider := build/fs-%/lib/pkgconfig/gioopenssl.pc
endif

ifneq ($(FRIDA_V8), disabled)
	v8 := build/fs-%/lib/pkgconfig/v8-$(v8_api_version).pc
endif

ifeq ($(host_os), $(filter $(host_os), macos ios))
ifneq ($(FRIDA_V8), disabled)
ifeq ($(FRIDA_ASAN), no)
	libcxx := build/fs-%/lib/c++/libc++.a
endif
endif
endif


.PHONY: all

all: build/sdk-$(host_os)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your SDK: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""


build/sdk-$(host_os)-$(host_arch).tar.bz2: build/fs-tmp-$(host_os_arch)/.package-stamp
	tar \
		-C build/fs-tmp-$(host_os_arch)/package \
		-cjf $(abspath $@.tmp) \
		.
	mv $@.tmp $@

build/fs-tmp-%/.package-stamp: \
		build/fs-%/lib/pkgconfig/liblzma.pc \
		build/fs-%/lib/pkgconfig/sqlite3.pc \
		$(unwind) \
		$(iconv) \
		$(elf) \
		$(dwarf) \
		build/fs-%/lib/pkgconfig/glib-2.0.pc \
		$(glib_tls_provider) \
		build/fs-%/lib/pkgconfig/gee-0.8.pc \
		build/fs-%/lib/pkgconfig/json-glib-1.0.pc \
		build/fs-%/lib/pkgconfig/libsoup-2.4.pc \
		build/fs-%/lib/pkgconfig/capstone.pc \
		build/fs-%/lib/pkgconfig/quickjs.pc \
		build/fs-%/lib/pkgconfig/libtcc.pc \
		$(v8) \
		$(libcxx)
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/fs-$* \
		&& [ -d lib/c++ ] && libcpp=lib/c++/*.a || libcpp= \
		&& [ -d lib/gio/modules ] && gio_modules=lib/gio/modules/*.a || gio_modules= \
		&& [ -d lib32 ] && lib32=lib32 || lib32= \
		&& [ -d lib64 ] && lib64=lib64 || lib64= \
		&& tar -c \
			include \
			lib/*.a \
			lib/*.la \
			lib/glib-2.0 \
			lib/libffi* \
			lib/pkgconfig \
			lib/tcc \
			$$libcpp \
			$$gio_modules \
			$$lib32 \
			$$lib64 \
			share/aclocal \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(abspath $(@D)/package) -xf -
	releng/relocatify.sh $(@D)/package $(abspath build/fs-$*) $(abspath releng)
ifeq ($(host_os), ios)
	cp $(shell xcrun --sdk macosx --show-sdk-path)/usr/include/mach/mach_vm.h \
		$(@D)/package/include/frida_mach_vm.h
endif
	@touch $@


.PHONY: libiconv

libiconv: build/fs-$(host_os_arch)/lib/libiconv.a

ext/.libiconv-stamp:
	$(call grab-and-prepare,libiconv)
	@touch $@

build/fs-tmp-%/libiconv/Makefile: build/fs-env-%.rc ext/.libiconv-stamp
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& ../../../ext/libiconv/configure $(libiconv_options)

build/fs-%/lib/libiconv.a: build/fs-env-%.rc build/fs-tmp-%/libiconv/Makefile
	. $< \
		&& cd build/fs-tmp-$*/libiconv \
		&& $(MAKE) $(MAKE_J) \
		&& $(MAKE) $(MAKE_J) install
	@touch $@


.PHONY: elfutils

elfutils: build/fs-$(host_os_arch)/lib/libelf.a

ext/.elfutils-stamp:
	$(call grab-and-prepare,elfutils)
	@touch $@

build/fs-tmp-%/elfutils/Makefile: build/fs-env-%.rc ext/.elfutils-stamp build/fs-%/lib/pkgconfig/liblzma.pc build/fs-%/lib/pkgconfig/zlib.pc
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& ../../../ext/elfutils/configure $(elfutils_options)

build/fs-%/lib/libelf.a: build/fs-env-%.rc build/fs-tmp-%/elfutils/Makefile
	. $< \
		&& cd build/fs-tmp-$*/elfutils \
		&& $(MAKE) $(MAKE_J) -C libelf libelf.a
	install -d build/fs-$*/include
	install -m 644 ext/elfutils/libelf/libelf.h build/fs-$*/include
	install -m 644 ext/elfutils/libelf/elf.h build/fs-$*/include
	install -m 644 ext/elfutils/libelf/gelf.h build/fs-$*/include
	install -m 644 ext/elfutils/libelf/nlist.h build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/elfutils/libelf/libelf.a build/fs-$*/lib
	@touch $@


.PHONY: libdwarf

libdwarf: build/fs-$(host_os_arch)/lib/libdwarf.a

ext/.libdwarf-stamp:
	$(call grab-and-prepare,libdwarf)
	@touch $@

build/fs-tmp-%/libdwarf/Makefile: build/fs-env-%.rc ext/.libdwarf-stamp build/fs-%/lib/libelf.a
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& ../../../ext/libdwarf/configure $(libdwarf_options)

build/fs-%/lib/libdwarf.a: build/fs-env-%.rc build/fs-tmp-%/libdwarf/Makefile
	. $< \
		&& $(MAKE) $(MAKE_J) -C build/fs-tmp-$*/libdwarf/libdwarf libdwarf.la
	install -d build/fs-$*/include
	install -m 644 libdwarf/libdwarf/dwarf.h build/fs-$*/include
	install -m 644 libdwarf/libdwarf/libdwarf.h build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/libdwarf/libdwarf/.libs/libdwarf.a build/fs-$*/lib
	@touch $@


define make-meson-module-rules
$(call make-meson-module-rules-for-env,$1,$2,$3,fs)
endef

define make-autotools-module-rules
$(call make-autotools-module-rules-for-env,$1,$2,$3,fs)
endef

$(eval $(call make-meson-module-rules,zlib,build/fs-%/lib/pkgconfig/zlib.pc,))

$(eval $(call make-autotools-module-rules,xz,build/fs-%/lib/pkgconfig/liblzma.pc,))

$(eval $(call make-meson-module-rules,sqlite,build/fs-%/lib/pkgconfig/sqlite3.pc,))

$(eval $(call make-autotools-module-rules,libunwind,build/fs-%/lib/pkgconfig/libunwind.pc, \
	build/fs-%/lib/pkgconfig/zlib.pc \
	build/fs-%/lib/pkgconfig/liblzma.pc \
))

$(eval $(call make-meson-module-rules,libffi,build/fs-%/lib/pkgconfig/libffi.pc,))

$(eval $(call make-meson-module-rules,glib,build/fs-%/lib/pkgconfig/glib-2.0.pc, \
	$(iconv) \
	build/fs-%/lib/pkgconfig/zlib.pc \
	build/fs-%/lib/pkgconfig/libffi.pc \
))

$(eval $(call make-meson-module-rules,glib-networking,build/fs-%/lib/pkgconfig/gioopenssl.pc, \
	build/fs-%/lib/pkgconfig/glib-2.0.pc build/fs-%/lib/pkgconfig/openssl.pc \
))

$(eval $(call make-meson-module-rules,libgee,build/fs-%/lib/pkgconfig/gee-0.8.pc, \
	build/fs-%/lib/pkgconfig/glib-2.0.pc \
))

$(eval $(call make-meson-module-rules,json-glib,build/fs-%/lib/pkgconfig/json-glib-1.0.pc, \
	build/fs-%/lib/pkgconfig/glib-2.0.pc \
))

$(eval $(call make-meson-module-rules,libpsl,build/fs-%/lib/pkgconfig/libpsl.pc,))

$(eval $(call make-meson-module-rules,libxml2,build/fs-%/lib/pkgconfig/libxml-2.0.pc, \
	build/fs-%/lib/pkgconfig/zlib.pc \
	build/fs-%/lib/pkgconfig/liblzma.pc \
))

$(eval $(call make-meson-module-rules,libsoup,build/fs-%/lib/pkgconfig/libsoup-2.4.pc, \
	build/fs-%/lib/pkgconfig/glib-2.0.pc \
	build/fs-%/lib/pkgconfig/sqlite3.pc \
	build/fs-%/lib/pkgconfig/libpsl.pc \
	build/fs-%/lib/pkgconfig/libxml-2.0.pc \
))

$(eval $(call make-meson-module-rules,capstone,build/fs-%/lib/pkgconfig/capstone.pc,))

$(eval $(call make-meson-module-rules,quickjs,build/fs-%/lib/pkgconfig/quickjs.pc,))

$(eval $(call make-meson-module-rules,tinycc,build/fs-%/lib/pkgconfig/libtcc.pc,))


ifeq ($(FRIDA_ASAN), yes)
	openssl_buildtype_args := \
		enable-asan \
		$(NULL)
else
	openssl_buildtype_args := \
		$(NULL)
endif

ifeq ($(host_os), $(filter $(host_os), macos ios))
	xcode_developer_dir := $(shell xcode-select -print-path)
ifeq ($(host_os_arch), macos-x86)
	openssl_arch_args := macos-i386
	xcode_platform := MacOSX
endif
ifeq ($(host_os_arch), macos-x86_64)
	openssl_arch_args := macos64-x86_64 enable-ec_nistp_64_gcc_128
	xcode_platform := MacOSX
endif
ifeq ($(host_os_arch), macos-arm64)
	openssl_arch_args := macos64-cross-arm64e enable-ec_nistp_64_gcc_128
	xcode_platform := MacOSX
endif
ifeq ($(host_os_arch), ios-x86)
	openssl_arch_args := ios-sim-cross-i386
	xcode_platform := iPhoneSimulator
endif
ifeq ($(host_os_arch), ios-x86_64)
	openssl_arch_args := ios-sim-cross-x86_64 enable-ec_nistp_64_gcc_128
	xcode_platform := iPhoneSimulator
endif
ifeq ($(host_os_arch), ios-arm)
	openssl_arch_args := ios-cross-armv7 -D__ARM_MAX_ARCH__=7
	xcode_platform := iPhoneOS
endif
ifeq ($(host_os_arch), ios-arm64)
	openssl_arch_args := ios64-cross-arm64 enable-ec_nistp_64_gcc_128
	xcode_platform := iPhoneOS
endif
ifeq ($(host_os_arch), ios-arm64e)
	openssl_arch_args := ios64-cross-arm64e enable-ec_nistp_64_gcc_128
	xcode_platform := iPhoneOS
endif
	openssl_host_env := \
		CPP=clang CC=clang CXX=clang++ LD= LDFLAGS= AR= RANLIB= \
		CROSS_COMPILE="$(xcode_developer_dir)/Toolchains/XcodeDefault.xctoolchain/usr/bin/" \
		CROSS_TOP="${xcode_developer_dir}/Platforms/$(xcode_platform).platform/Developer" \
		CROSS_SDK=$(xcode_platform)$(shell xcrun --sdk $(shell echo $(xcode_platform) | tr A-Z a-z) --show-sdk-version | cut -f1-2 -d'.').sdk \
		IOS_MIN_SDK_VERSION=8.0 \
		CONFIG_DISABLE_BITCODE=true \
		$(NULL)
ifeq ($(host_os_arch), macos-x86)
ifneq ($(MACOS_X86_SDK_ROOT),)
	openssl_host_env += MACOS_SDK_ROOT="$(MACOS_X86_SDK_ROOT)"
endif
endif
ifeq ($(host_os_arch), $(filter $(host_os_arch), macos-arm64 macos-arm64e))
	openssl_host_env += MACOS_MIN_SDK_VERSION=11.0
else
	openssl_host_env += MACOS_MIN_SDK_VERSION=10.9
endif
endif
ifeq ($(host_os), linux)
ifeq ($(host_arch), x86)
	openssl_arch_args := linux-x86
endif
ifeq ($(host_arch), x86_64)
	openssl_arch_args := linux-x86_64 enable-ec_nistp_64_gcc_128
endif
ifeq ($(host_arch), $(filter $(host_arch), arm armbe8 armeabi armhf))
	openssl_arch_args := linux-armv4
endif
ifeq ($(host_arch), arm64)
	openssl_arch_args := linux-aarch64
endif
ifeq ($(host_arch), $(filter $(host_arch), mips mipsel))
	openssl_arch_args := linux-mips32
endif
ifeq ($(host_arch), $(filter $(host_arch), mips64 mips64el))
	openssl_arch_args := linux-mips64
endif
	openssl_host_env := \
		$(NULL)
endif
ifeq ($(host_os), android)
ifeq ($(host_arch), x86)
	openssl_arch_args := android-x86 -D__ANDROID_API__=18
	ndk_abi := x86
	ndk_triplet := i686-linux-android
endif
ifeq ($(host_arch), x86_64)
	openssl_arch_args := android-x86_64 -D__ANDROID_API__=21
	ndk_abi := x86_64
	ndk_triplet := x86_64-linux-android
endif
ifeq ($(host_arch), arm)
	openssl_arch_args := android-arm -D__ANDROID_API__=18 -D__ARM_MAX_ARCH__=7 -fno-integrated-as
	ndk_abi := arm-linux-androideabi
	ndk_triplet := arm-linux-androideabi
endif
ifeq ($(host_arch), arm64)
	openssl_arch_args := android-arm64 -D__ANDROID_API__=21
	ndk_abi := aarch64-linux-android
	ndk_triplet := aarch64-linux-android
endif
	ndk_build_os_arch := $(shell uname -s | tr '[A-Z]' '[a-z]')-$(build_arch)
	ndk_llvm_prefix := $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ndk_build_os_arch)
	ndk_gcc_prefix := $(ANDROID_NDK_ROOT)/toolchains/$(ndk_abi)-4.9/prebuilt/$(ndk_build_os_arch)
	openssl_host_env := \
		CPP=clang CC=clang CXX=clang++ LD= LDFLAGS= AR=$(ndk_triplet)-ar RANLIB=$(ndk_triplet)-ranlib \
		ANDROID_NDK=$(ANDROID_NDK_ROOT) \
		PATH=$(ndk_llvm_prefix)/bin:$(ndk_gcc_prefix)/bin:$$PATH \
		$(NULL)
endif

.PHONY: openssl

openssl: build/fs-$(host_os_arch)/lib/pkgconfig/openssl.pc

ext/.openssl-stamp:
	$(call grab-and-prepare,openssl)
	@touch $@

build/fs-tmp-%/openssl/Configure: ext/.openssl-stamp
	$(RM) -r $(@D)
	mkdir -p build/fs-tmp-$*
	cp -a ext/openssl $(@D)
	@touch $@

build/fs-%/lib/pkgconfig/openssl.pc: build/fs-env-%.rc build/fs-tmp-%/openssl/Configure
	. $< \
		&& . $$CONFIG_SITE \
		&& export CC CFLAGS \
		&& export $(openssl_host_env) OPENSSL_LOCAL_CONFIG_DIR="$(abspath releng/openssl-config)" \
		&& cd build/fs-tmp-$*/openssl \
		&& perl Configure \
			--prefix=$$frida_prefix \
			$(openssl_options) \
			$(openssl_buildtype_args) \
			$(openssl_arch_args) \
		&& $(MAKE) depend \
		&& $(MAKE) build_libs \
		&& $(MAKE) install_dev


ifeq ($(FRIDA_ASAN), yes)
v8_buildtype_args := \
	is_asan=true \
	symbol_level=1 \
	$(NULL)
else
v8_buildtype_args := \
	is_official_build=true \
	chrome_pgo_phase=0 \
	is_debug=false \
	v8_enable_v8_checks=false \
	symbol_level=0 \
	$(NULL)
endif

ifeq ($(host_arch), x86)
	v8_cpu := x86
endif
ifeq ($(host_arch), x86_64)
	v8_cpu := x64
endif
ifeq ($(host_arch), $(filter $(host_arch), arm armbe8 armeabi armhf))
	v8_cpu := arm
	v8_cpu_args := arm_version=7 arm_fpu="vfpv3-d16" arm_use_neon=false
ifneq ($(host_os), android)
ifeq ($(host_arch), armhf)
	v8_cpu_args += arm_float_abi="hard"
else
	v8_cpu_args += arm_float_abi="softfp"
endif
endif
endif
ifeq ($(host_arch), arm64)
	v8_cpu := arm64
endif
ifeq ($(host_arch), arm64e)
	v8_cpu := arm64
	v8_cpu_args := arm_version=83
endif

v8_build_os := $(shell echo $(build_os) | sed 's,^macos$$,mac,')
ifeq ($(host_os), macos)
	v8_os := mac
	v8_platform_args := $(NULL)
ifeq ($(host_arch), $(filter $(host_arch), arm64 arm64e))
	v8_platform_args += mac_deployment_target="11.0"
else
	v8_platform_args += mac_deployment_target="10.9"
endif
ifeq ($(FRIDA_ASAN), yes)
	v8_platform_args += use_xcode_clang=true
else
	v8_platform_args += use_xcode_clang=false
endif
endif
ifeq ($(host_os), ios)
	v8_os := ios
	v8_platform_args := \
		use_xcode_clang=true \
		mac_deployment_target="10.9" \
		ios_deployment_target="8.0" \
		$(NULL)
endif
ifeq ($(host_os), $(filter $(host_os), macos ios))
ifeq ($(host_arch), $(filter $(host_arch), arm64 arm64e))
	v8_platform_args += v8_enable_pointer_compression=false
endif
endif
ifeq ($(host_os), linux)
	v8_os := linux
	v8_platform_args := \
		is_clang=false \
		is_cfi=false \
		use_sysroot=false \
		linux_use_bundled_binutils=false \
		use_gold=false
	v8_libs_private := "-lrt"
endif
ifeq ($(host_os), android)
	v8_os := android
	v8_platform_args := \
		use_xcode_clang=true \
		use_custom_libcxx=false \
		android_ndk_root="$(ANDROID_NDK_ROOT)" \
		android_ndk_version="r21" \
		android_ndk_major_version=21 \
		android32_ndk_api_level=18 \
		android64_ndk_api_level=21 \
		clang_base_path="$(abspath $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ndk_build_os_arch))"
	v8_libs_private := "-llog -lm"
endif

ifeq ($(host_os), $(filter $(host_os), linux android))
	v8_platform_args += enable_resource_allowlist_generation=false
endif

ifeq ($(host_arch), $(filter $(host_arch), x86 arm))
ifneq ($(MACOS_X86_SDK_ROOT),)
	v8_platform_args += mac_sdk_path="$(MACOS_X86_SDK_ROOT)"
endif
endif
ifneq ($(IOS_SDK_ROOT),)
	v8_platform_args += ios_sdk_path="$(IOS_SDK_ROOT)"
endif

.PHONY: v8 gn depot_tools

v8: build/fs-$(host_os_arch)/lib/pkgconfig/v8-$(v8_api_version).pc
gn: build/fs-tmp-$(build_os_arch)/gn/gn
depot_tools: ext/.depot_tools-stamp

ext/.gn-stamp:
	# Google's prebuilt GN requires a newer glibc than our Debian Squeeze buildroot has.
	$(call grab-and-prepare,gn)
	@touch $@

build/fs-tmp-%/gn/build.ninja: build/fs-env-%.rc ext/.gn-stamp
	. $< \
		&& CC="$$CC" CXX="$$CXX" python ext/gn/build/gen.py \
			--out-path $(abspath $(@D)) \
			$(gn_options)

build/fs-tmp-%/gn/gn: build/fs-tmp-%/gn/build.ninja
	$(NINJA) -C build/fs-tmp-$*/gn
	@touch $@

ext/.depot_tools-stamp:
	$(call grab-and-prepare,depot_tools)
	@touch $@

ext/v8-checkout/.gclient: ext/.depot_tools-stamp
	cd ext/v8-checkout \
		&& PATH="$(abspath ext/depot_tools):$$PATH" \
			gclient config --spec 'solutions = [ \
  { \
    "url": "$(v8_url)@$(v8_version)", \
    "managed": False, \
    "name": "v8", \
    "deps_file": "DEPS", \
    "custom_deps": {}, \
  }, \
]'

ext/v8-checkout/v8: ext/v8-checkout/.gclient
	cd ext/v8-checkout \
		&& PATH="$(abspath ext/depot_tools):$$PATH" \
			gclient sync
	@touch $@

build/fs-tmp-%/v8/build.ninja: ext/v8-checkout/v8 build/fs-tmp-$(build_os_arch)/gn/gn
	cd ext/v8-checkout/v8 \
		&& ../../../build/fs-tmp-$(build_os_arch)/gn/gn \
			gen $(abspath $(@D)) \
			--args=' \
				target_os="$(v8_os)" \
				target_cpu="$(v8_cpu)" \
				$(v8_cpu_args) \
				$(v8_buildtype_args) \
				$(v8_platform_args) \
				$(v8_options) \
			'

build/fs-tmp-%/v8/obj/libv8_monolith.a: build/fs-tmp-%/v8/build.ninja
	$(NINJA) -C build/fs-tmp-$*/v8 v8_monolith
	@touch $@

build/fs-%/lib/pkgconfig/v8-$(v8_api_version).pc: build/fs-tmp-%/v8/obj/libv8_monolith.a
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8
	install -m 644 ext/v8-checkout/v8/include/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/inspector
	install -m 644 build/fs-tmp-$*/v8/gen/include/inspector/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/inspector/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/libplatform
	install -m 644 ext/v8-checkout/v8/include/libplatform/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/libplatform/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc
	install -m 644 ext/v8-checkout/v8/include/cppgc/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/internal
	install -m 644 ext/v8-checkout/v8/include/cppgc/internal/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/internal/
	install -d build/fs-$*/lib
	install -m 644 $< build/fs-$*/lib/libv8-$(v8_api_version).a
	install -d $(@D)
	$(PYTHON3) releng/v8.py \
		patch build/fs-$*/include/v8-$(v8_api_version)/v8/v8config.h \
		-s ext/v8-checkout/v8 \
		-b build/fs-tmp-$*/v8 \
		-G build/fs-tmp-$(build_os_arch)/gn/gn
	echo "prefix=\$${frida_sdk_prefix}" > $@.tmp
	echo "libdir=\$${prefix}/lib" >> $@.tmp
	echo "includedir=\$${prefix}/include/v8-$(v8_api_version)" >> $@.tmp
	echo "" >> $@.tmp
	echo "Name: V8" >> $@.tmp
	echo "Description: V8 JavaScript Engine" >> $@.tmp
	echo "Version: $$($(PYTHON3) releng/v8.py get version -s ext/v8-checkout/v8)" >> $@.tmp
	echo "Libs: -L\$${libdir} -lv8-$(v8_api_version)" >> $@.tmp
ifdef v8_libs_private
	echo Libs.private: $(v8_libs_private) >> $@.tmp
endif
	echo "Cflags: -I\$${includedir} -I\$${includedir}/v8" >> $@.tmp
	mv $@.tmp $@


.PHONY: libcxx

libcxx: build/fs-$(host_os_arch)/lib/c++/libc++.a

build/fs-%/lib/c++/libc++.a: build/fs-tmp-%/v8/obj/libv8_monolith.a
	$(NINJA) -C build/fs-tmp-$*/v8 libc++
	install -d build/fs-$*/include/c++/
	cp -a ext/v8-checkout/v8/buildtools/third_party/libc++/trunk/include/* build/fs-$*/include/c++/
	rm build/fs-$*/include/c++/CMakeLists.txt build/fs-$*/include/c++/__config_site.in
	( \
		echo "#ifndef _LIBCPP_CONFIG_SITE"; \
		echo "#define _LIBCPP_CONFIG_SITE"; \
		echo ""; \
		echo "#define _LIBCPP_HAS_NO_ALIGNED_ALLOCATION"; \
		echo "#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS"; \
		echo "#define _LIBCPP_ENABLE_NODISCARD"; \
		echo ""; \
		echo "#endif"; \
		echo ""; \
	) | cat \
		- \
		ext/v8-checkout/v8/buildtools/third_party/libc++/trunk/include/__config \
		> build/fs-$*/include/c++/__config
	install -d build/fs-$*/lib/c++
	$(shell xcrun -f libtool) -static -no_warning_for_no_symbols \
		-o build/fs-$*/lib/c++/libc++abi.a \
		build/fs-tmp-$*/v8/obj/buildtools/third_party/libc++abi/libc++abi/*.o
	$(shell xcrun -f libtool) -static -no_warning_for_no_symbols \
		-o build/fs-$*/lib/c++/libc++.a \
		build/fs-tmp-$*/v8/obj/buildtools/third_party/libc++/libc++/*.o


build/fs-env-%.rc:
	FRIDA_HOST=$* \
		FRIDA_ACOPTFLAGS="$(FRIDA_ACOPTFLAGS_BOTTLE)" \
		FRIDA_ACDBGFLAGS="$(FRIDA_ACDBGFLAGS_BOTTLE)" \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		FRIDA_ENV_NAME=fs \
		FRIDA_ENV_SDK=none \
		FRIDA_TOOLCHAIN_VERSION=$(frida_bootstrap_version) \
		FRIDA_SDK_VERSION=$(frida_bootstrap_version) \
		./releng/setup-env.sh

releng/meson/meson.py:
	git submodule init releng/meson
	git submodule update releng/meson
	@touch $@


.SECONDARY:
