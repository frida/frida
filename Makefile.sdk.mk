include config.mk
include releng/deps.mk


MAKE_J ?= -j 8
SHELL = /bin/bash


packages = \
	zlib \
	xz \
	sqlite \
	libffi \
	glib \
	libgee \
	json-glib \
	libsoup \
	capstone \
	quickjs \
	tinycc \
	$(NULL)


ifeq ($(host_os), $(filter $(host_os), macos ios))
# Pull in iconv so our payloads only depend on libSystem.
glib_deps += libiconv
endif

ifeq ($(host_os), $(filter $(host_os), linux android qnx))
packages += elfutils libdwarf libunwind
endif

ifeq ($(host_os), $(filter $(host_os), macos ios linux android))
packages += glib-networking
endif

ifneq ($(FRIDA_V8), disabled)
packages += v8
ifeq ($(host_os), $(filter $(host_os), macos ios))
ifeq ($(FRIDA_ASAN), no)
packages += libcxx
endif
endif
endif


.PHONY: all clean distclean

all: build/sdk-$(host_os)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your SDK: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""

clean: $(foreach pkg, $(call expand-packages,$(packages)), clean-$(pkg))

distclean: $(foreach pkg, $(call expand-packages,$(packages)), distclean-$(pkg))


build/sdk-$(host_os)-$(host_arch).tar.bz2: build/fs-tmp-$(host_os_arch)/.package-stamp
	@$(call print-status,📦,Compressing)
	@tar \
		-C build/fs-tmp-$(host_os_arch)/package \
		-cjf $(abspath $@.tmp) \
		.
	@mv $@.tmp $@

build/fs-tmp-%/.package-stamp: $(foreach pkg, $(packages), build/fs-%/manifest/$(pkg).pkg)
	@echo
	@$(call print-status,📦,Assembling)
	@$(RM) -r $(@D)/package
	@mkdir -p $(@D)/package
	@cd build/fs-$* \
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
			manifest \
			share/aclocal \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(abspath $(@D)/package) -xf -
	@releng/pkgify.sh $(@D)/package $(abspath build/fs-$*) $(abspath releng)
ifeq ($(host_os), ios)
	@cp $(shell xcrun --sdk macosx --show-sdk-path)/usr/include/mach/mach_vm.h \
		$(@D)/package/include/frida_mach_vm.h
endif
	@echo "$(frida_sdk_version)" > $(@D)/package/VERSION.txt
	@touch $@


$(eval $(call make-package-rules,$(packages),fs))


libelf_headers = \
	libelf.h \
	elf.h \
	gelf.h \
	nlist.h \
	$(NULL)

$(eval $(call make-autotools-package-rules-without-build-rule,elfutils,fs))

build/fs-%/manifest/elfutils.pkg: build/fs-env-%.rc build/fs-tmp-%/elfutils/Makefile
	@$(call print-status,elfutils,Building)
	@prefix=build/fs-$*; \
	builddir=build/fs-tmp-$*/elfutils; \
	(set -x \
		&& . $< \
		&& $(MAKE) $(MAKE_J) -C $$builddir/libelf libelf.a \
		&& install -d $$prefix/include \
		&& for header in $(libelf_headers); do \
			install -m 644 deps/elfutils/libelf/$$header $$prefix/include; \
		done \
		&& install -d $$prefix/lib \
		&& install -m 644 $$builddir/libelf/libelf.a $$prefix/lib \
	) >>$$builddir/build.log 2>&1
	@$(call print-status,elfutils,Generating manifest)
	@( \
		for header in $(libelf_headers); do \
			echo "include/$$header"; \
		done; \
		echo "lib/libelf.a" \
	) | sort > $@


libdwarf_headers = \
	dwarf.h \
	libdwarf.h \
	$(NULL)

$(eval $(call make-autotools-package-rules-without-build-rule,libdwarf,fs))

build/fs-%/manifest/libdwarf.pkg: build/fs-env-%.rc build/fs-tmp-%/libdwarf/Makefile
	@$(call print-status,libdwarf,Building)
	@prefix=build/fs-$*; \
	builddir=build/fs-tmp-$*/libdwarf; \
	(set -x \
		&& . $< \
		&& $(MAKE) $(MAKE_J) -C $$builddir/libdwarf libdwarf.la \
		&& install -d $$prefix/include \
		&& for header in $(libdwarf_headers); do \
			install -m 644 deps/libdwarf/libdwarf/$$header $$prefix/include; \
		done \
		&& install -d $$prefix/lib \
		&& install -m 644 $$builddir/libdwarf/.libs/libdwarf.a $$prefix/lib \
	) >>$$builddir/build.log 2>&1
	@$(call print-status,libdwarf,Generating manifest)
	@( \
		for header in $(libdwarf_headers); do \
			echo "include/$$header"; \
		done; \
		echo "lib/libdwarf.a" \
	) | sort > $@


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
endif
ifeq ($(host_os_arch), macos-x86_64)
openssl_arch_args := macos64-x86_64 enable-ec_nistp_64_gcc_128
endif
ifeq ($(host_os_arch), macos-arm64)
openssl_arch_args := macos64-cross-arm64 enable-ec_nistp_64_gcc_128
endif
ifeq ($(host_os_arch), macos-arm64e)
openssl_arch_args := macos64-cross-arm64e enable-ec_nistp_64_gcc_128
endif
ifeq ($(host_os), macos)
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

$(eval $(call make-base-package-rules,openssl,fs,$(host_os_arch)))

deps/.openssl-stamp:
	$(call grab-and-prepare,openssl)
	@touch $@

build/fs-tmp-%/openssl/Configure: deps/.openssl-stamp
	@$(call print-status,openssl,Setting up build directory)
	@$(RM) -r $(@D)
	@mkdir -p build/fs-tmp-$*
	@cp -a deps/openssl $(@D)
	@touch $@

build/fs-%/manifest/openssl.pkg: build/fs-env-%.rc build/fs-tmp-%/openssl/Configure \
		$(foreach dep, $(openssl_deps), build/fs-%/manifest/$(dep).pkg) \
		$(foreach dep, $(openssl_deps_for_build), build/fs-$(build_os_arch)/manifest/$(dep).pkg)
	@$(call print-status,openssl,Building)
	@builddir=build/fs-tmp-$*/openssl; \
	(set -x \
		&& . $< \
		&& . $$CONFIG_SITE \
		&& export CC CFLAGS \
		&& export $(openssl_host_env) OPENSSL_LOCAL_CONFIG_DIR="$(abspath releng/openssl-config)" \
		&& cd $$builddir \
		&& perl Configure \
			--prefix=$$frida_prefix \
			$(openssl_options) \
			$(openssl_buildtype_args) \
			$(openssl_arch_args) \
		&& $(MAKE) depend \
		&& $(MAKE) build_libs \
		&& $(MAKE) install_dev \
	) >$$builddir/build.log 2>&1 \
	&& $(call print-status,openssl,Generating manifest) \
	&& (set -x; \
		$(call make-autotools-manifest-commands,openssl,fs,$*,install_dev) \
	) >>$$builddir/build.log 2>&1


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
v8_platform_args := \
	use_xcode_clang=true \
	$(NULL)
ifeq ($(host_arch), $(filter $(host_arch), arm64 arm64e))
v8_platform_args += mac_deployment_target="11.0"
else
v8_platform_args += mac_deployment_target="10.9"
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
	use_gold=false
v8_libs_private := -lrt
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
v8_libs_private := -llog -lm
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

# Google's prebuilt GN requires a newer glibc than our Debian Squeeze buildroot has.

$(eval $(call make-base-package-rules,gn,fs,$(build_os_arch)))

deps/.gn-stamp:
	$(call grab-and-prepare,gn)
	@touch $@

build/fs-tmp-%/gn/build.ninja: build/fs-env-%.rc deps/.gn-stamp \
		$(foreach dep, $(gn_deps), build/fs-%/manifest/$(dep).pkg) \
		$(foreach dep, $(gn_deps_for_build), build/fs-$(build_os_arch)/manifest/$(dep).pkg)
	@$(call print-status,gn,Configuring)
	@$(RM) -r $(@D)
	@mkdir -p $(@D)
	@(set -x \
		&& . $< \
		&& CC="$$CC" CXX="$$CXX" python deps/gn/build/gen.py \
			--out-path $(abspath $(@D)) \
			$(gn_options) \
	) >$(@D)/build.log 2>&1

build/fs-%/manifest/gn.pkg: build/fs-tmp-%/gn/build.ninja
	@$(call print-status,gn,Building)
	@prefix=build/fs-$*; \
	builddir=build/fs-tmp-$*/gn; \
	(set -x \
		&& $(NINJA) -C $$builddir \
		&& install -d $$prefix/bin \
		&& install -m 755 $$builddir/gn $$prefix/bin \
	) >>$$builddir/build.log 2>&1
	@$(call print-status,gn,Generating manifest)
	@mkdir -p $(@D)
	@echo "bin/gn" > $@

.PHONY: depot_tools clean-depot_tools distclean-depot_tools

depot_tools: deps/.depot_tools-stamp

clean-depot_tools:

distclean-depot_tools: clean-depot_tools
	$(RM) deps/.depot_tools-stamp
	$(RM) -r deps/depot_tools

deps/.depot_tools-stamp: \
		$(foreach dep, $(depot_tools_deps), build/fs-$(build_os_arch)/manifest/$(dep).pkg) \
		$(foreach dep, $(depot_tools_deps_for_build), build/fs-$(build_os_arch)/manifest/$(dep).pkg)
	$(call grab-and-prepare,depot_tools)
	@echo '{"is-googler": false, "countdown": 10, "opt-in": null, "version": 1}' > deps/depot_tools/metrics.cfg
	@touch $@

$(eval $(call make-base-package-rules,v8,fs,$(host_os_arch)))

deps/v8-checkout/.gclient: deps/.depot_tools-stamp
	@$(call print-repo-banner,v8,$(v8_version),$(v8_url))
	@mkdir -p $(@D)
	@cd deps/v8-checkout \
		&& PATH="$(abspath deps/depot_tools):$$PATH" \
			gclient config --spec 'solutions = [ \
  { \
    "url": "$(v8_url)@$(v8_version)", \
    "managed": False, \
    "name": "v8", \
    "deps_file": "DEPS", \
    "custom_deps": {}, \
  }, \
]'

deps/v8-checkout/v8: deps/v8-checkout/.gclient
	@$(call print-status,v8,Cloning into deps/v8-checkout)
	@cd deps/v8-checkout \
		&& PATH="$(abspath deps/depot_tools):$$PATH" \
			gclient sync
	@touch $@

build/fs-tmp-%/v8/build.ninja: deps/v8-checkout/v8 build/fs-$(build_os_arch)/manifest/gn.pkg \
		$(foreach dep, $(v8_deps), build/fs-%/manifest/$(dep).pkg) \
		$(foreach dep, $(v8_deps_for_build), build/fs-$(build_os_arch)/manifest/$(dep).pkg)
	@$(call print-status,v8,Configuring)
	@$(RM) -r $(@D)
	@mkdir -p $(@D)
	@(set -x \
		&& cd deps/v8-checkout/v8 \
		&& ../../../build/fs-$(build_os_arch)/bin/gn \
			gen $(abspath $(@D)) \
			--args='$(strip \
				target_os="$(v8_os)" \
				target_cpu="$(v8_cpu)" \
				$(v8_cpu_args) \
				$(v8_buildtype_args) \
				$(v8_platform_args) \
				$(v8_options) \
			)' \
	) >$(@D)/build.log 2>&1

build/fs-%/manifest/v8.pkg: build/fs-tmp-%/v8/build.ninja
	@$(call print-status,v8,Building)
	@prefix=build/fs-$*; \
	srcdir=deps/v8-checkout/v8; \
	builddir=build/fs-tmp-$*/v8; \
	(set -x \
		&& $(NINJA) -C $$builddir v8_monolith \
		&& install -d $$prefix/include/v8-$(v8_api_version)/v8 \
		&& install -m 644 $$srcdir/include/*.h $$prefix/include/v8-$(v8_api_version)/v8/ \
		&& install -d $$prefix/include/v8-$(v8_api_version)/v8/inspector \
		&& install -m 644 $$builddir/gen/include/inspector/*.h $$prefix/include/v8-$(v8_api_version)/v8/inspector/ \
		&& install -d $$prefix/include/v8-$(v8_api_version)/v8/libplatform \
		&& install -m 644 $$srcdir/include/libplatform/*.h $$prefix/include/v8-$(v8_api_version)/v8/libplatform/ \
		&& install -d $$prefix/include/v8-$(v8_api_version)/v8/cppgc \
		&& install -m 644 $$srcdir/include/cppgc/*.h $$prefix/include/v8-$(v8_api_version)/v8/cppgc/ \
		&& install -d $$prefix/include/v8-$(v8_api_version)/v8/cppgc/internal \
		&& install -m 644 $$srcdir/include/cppgc/internal/*.h $$prefix/include/v8-$(v8_api_version)/v8/cppgc/internal/ \
		&& install -d $$prefix/lib \
		&& install -m 644 $$builddir/obj/libv8_monolith.a $$prefix/lib/libv8-$(v8_api_version).a \
		&& $(PYTHON3) releng/v8.py \
			patch $$prefix/include/v8-$(v8_api_version)/v8/v8config.h \
			-s $$srcdir \
			-b $$builddir \
			-G build/fs-$(build_os_arch)/bin/gn \
		&& install -d $$prefix/lib/pkgconfig \
		&& ( \
			echo "prefix=\$${frida_sdk_prefix}"; \
			echo "libdir=\$${prefix}/lib"; \
			echo "includedir=\$${prefix}/include/v8-$(v8_api_version)"; \
			echo ""; \
			echo "Name: V8"; \
			echo "Description: V8 JavaScript Engine"; \
			echo "Version: $$($(PYTHON3) releng/v8.py get version -s $$srcdir)"; \
			echo "Libs: -L\$${libdir} -lv8-$(v8_api_version)"; \
			$(if $(v8_libs_private),echo "Libs.private: $(v8_libs_private)";,) \
			echo "Cflags: -I\$${includedir} -I\$${includedir}/v8" \
		) > $$prefix/lib/pkgconfig/v8-$(v8_api_version).pc \
	) >>$$builddir/build.log 2>&1 \
	&& $(call print-status,v8,Generating manifest) \
	&& ( \
		cd $$prefix; \
		find include/v8-$(v8_api_version) -type f; \
		echo "lib/libv8-$(v8_api_version).a"; \
		echo "lib/pkgconfig/v8-$(v8_api_version).pc" \
	) | sort > $@


$(eval $(call make-base-package-rules,libcxx,fs,$(host_os_arch)))

build/fs-%/manifest/libcxx.pkg: build/fs-%/manifest/v8.pkg
	@$(call print-status,libcxx,Building)
	@prefix=build/fs-$*; \
	srcdir=deps/v8-checkout/v8; \
	builddir=build/fs-tmp-$*/v8; \
	(set -x \
		&& $(NINJA) -C $$builddir libc++ \
		&& install -d $$prefix/include/c++/ \
		&& cp -a $$srcdir/buildtools/third_party/libc++/trunk/include/* $$prefix/include/c++/ \
		&& rm $$prefix/include/c++/CMakeLists.txt $$prefix/include/c++/__config_site.in \
		&& ( \
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
			$$srcdir/buildtools/third_party/libc++/trunk/include/__config \
			> $$prefix/include/c++/__config \
		&& install -d $$prefix/lib/c++ \
		&& $(shell xcrun -f libtool) -static -no_warning_for_no_symbols \
			-o $$prefix/lib/c++/libc++abi.a \
			$$builddir/obj/buildtools/third_party/libc++abi/libc++abi/*.o \
		&& $(shell xcrun -f libtool) -static -no_warning_for_no_symbols \
			-o $$prefix/lib/c++/libc++.a \
			$$builddir/obj/buildtools/third_party/libc++/libc++/*.o \
	) >$$builddir/libcxx-build.log 2>&1 \
	&& $(call print-status,libcxx,Generating manifest) \
	&& ( \
		cd $$prefix; \
		find include/c++ -type f; \
		find lib/c++ -type f; \
	) | sort > $@


build/fs-env-%.rc:
	@FRIDA_HOST=$* \
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
