include config.mk
include releng/deps.mk


MAKE_J ?= -j 8
SHELL := $(shell which bash)

ifeq ($(FRIDA_V8), auto)
FRIDA_V8 := $(shell echo $(host_os_arch) | grep -Evq "^qnx-" && echo "enabled" || echo "disabled")
endif


packages = \
	zlib \
	xz \
	brotli \
	minizip \
	sqlite \
	libffi \
	glib \
	glib-networking \
	libnice \
	usrsctp \
	libgee \
	json-glib \
	libsoup \
	capstone \
	quickjs \
	$(NULL)


ifeq ($(host_os), $(filter $(host_os), macos ios))
# Pull in iconv so our payloads only depend on libSystem.
glib_deps += libiconv
endif

ifeq ($(host_os), $(filter $(host_os), linux android qnx))
packages += elfutils libdwarf libunwind
endif

ifeq ($(host_os), freebsd)
packages += libdwarf libunwind
endif

ifeq ($(host_os), android)
packages += selinux
endif

ifneq ($(FRIDA_V8), disabled)
packages += v8
ifeq ($(host_os), $(filter $(host_os), macos ios))
ifeq ($(FRIDA_ASAN), no)
packages += libcxx
endif
endif
endif

ifeq ($(host_arch), $(filter $(host_arch), x86 x86_64 arm armbe8 armeabi armhf arm64 arm64e arm64eoabi))
packages += tinycc
endif

ifeq ($(host_os_arch), ios-arm64eoabi)
xcode_env_setup := export DEVELOPER_DIR="$(XCODE11)/Contents/Developer"
xcode_run := arch -x86_64 xcrun
else
xcode_env_setup := true
xcode_run := xcrun
endif
ifeq ($(host_os), macos)
xcode_platform := MacOSX
endif
ifeq ($(host_os), ios)
ifeq ($(host_arch), $(filter $(host_arch), x86 x86_64))
xcode_platform := iPhoneSimulator
else
xcode_platform := iPhoneOS
endif
endif
ifeq ($(host_os), $(filter $(host_os), macos ios))
xcode_developer_dir := $(shell $(xcode_env_setup); xcode-select -print-path)
xcode_sdk_version := $(shell $(xcode_env_setup); $(xcode_run) --sdk $(shell echo $(xcode_platform) | tr A-Z a-z) --show-sdk-version | cut -f1-2 -d'.')
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
		-cjf $(shell pwd)/$@.tmp \
		.
	@mv $@.tmp $@

build/fs-tmp-%/.package-stamp: $(foreach pkg, $(packages), build/fs-%/manifest/$(pkg).pkg)
	@echo
	@$(call print-status,📦,Assembling)
	@$(RM) -r $(@D)/package
	@mkdir -p $(@D)/package
	@cd build/fs-$* \
		&& [ -d lib/tcc ] && tcc=lib/tcc || tcc= \
		&& [ -d lib/c++ ] && libcpp=lib/c++/*.a || libcpp= \
		&& [ -d lib/gio/modules ] && gio_modules=lib/gio/modules/*.a || gio_modules= \
		&& [ -d lib32 ] && lib32=lib32 || lib32= \
		&& [ -d lib64 ] && lib64=lib64 || lib64= \
		&& [ -d libdata ] && libdatadir=libdata || libdatadir=lib \
		&& tar -cf - \
			include \
			lib/*.a \
			lib/glib-2.0 \
			lib/libffi* \
			$$libdatadir/pkgconfig \
			$$tcc \
			$$libcpp \
			$$gio_modules \
			$$lib32 \
			$$lib64 \
			manifest \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(shell pwd)/$(@D)/package -xf -
	@releng/pkgify.sh "$(@D)/package" "$(shell pwd)/build/fs-$*" "$(shell pwd)/releng"
	@echo "$(frida_deps_version)" > $(@D)/package/VERSION.txt
	@touch $@


$(eval $(call make-package-rules,$(packages),fs))


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
ifeq ($(host_arch), $(filter $(host_arch), arm64e arm64eoabi))
v8_cpu := arm64
v8_cpu_args := arm_version=83
endif
ifeq ($(host_arch), mips)
v8_cpu := mips
endif
ifeq ($(host_arch), mipsel)
v8_cpu := mipsel
endif
ifeq ($(host_arch), mips64)
v8_cpu := mips64
endif
ifeq ($(host_arch), mips64el)
v8_cpu := mips64el
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
ifeq ($(host_arch), $(filter $(host_arch), arm64 arm64e arm64eoabi))
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
endif
ifeq ($(host_os), android)
ifeq ($(build_os_arch), macos-arm64)
# NDK does not yet support Apple Silicon.
ndk_build_os_arch := darwin-x86_64
else
ndk_build_os_arch := $(shell uname -s | tr '[A-Z]' '[a-z]')-$(build_arch)
endif
v8_os := android
v8_platform_args := \
	use_xcode_clang=true \
	use_custom_libcxx=false \
	android_ndk_root="$(ANDROID_NDK_ROOT)" \
	android_ndk_version="r21" \
	android_ndk_major_version=21 \
	android32_ndk_api_level=19 \
	android64_ndk_api_level=21 \
	clang_base_path="$(abspath $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ndk_build_os_arch))"
endif

ifeq ($(host_os), $(filter $(host_os), linux android))
v8_platform_args += enable_resource_allowlist_generation=false
endif

ifneq ($(MACOS_X86_SDK_ROOT),)
ifeq ($(build_os), macos)
ifeq ($(host_arch), $(filter $(host_arch), x86 arm))
v8_platform_args += mac_sdk_path="$(MACOS_X86_SDK_ROOT)"
endif
endif
endif
ifneq ($(IOS_SDK_ROOT),)
ifeq ($(host_os), ios)
v8_platform_args += ios_sdk_path="$(IOS_SDK_ROOT)"
endif
endif
ifeq ($(host_os), freebsd)
v8_os := freebsd
endif

depot_tools_config := \
	DEPOT_TOOLS_UPDATE=0 \
	VPYTHON_BYPASS="manually managed python not supported by chrome operations" \
	$(NULL)

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
			--out-path $(shell pwd)/$(@D) \
			$(gn_options) \
	) >$(@D)/build.log 2>&1

build/fs-%/manifest/gn.pkg: build/fs-tmp-%/gn/build.ninja
	@$(call print-status,gn,Building)
	@prefix=build/fs-$*; \
	builddir=build/fs-tmp-$*/gn; \
	(set -x \
		&& . build/fs-env-$*.rc \
		&& ninja -C $$builddir \
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
		&& export PATH="$(shell pwd)/deps/depot_tools:$$PATH" $(depot_tools_config) \
		&& gclient config --spec 'solutions = [ \
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
		&& export PATH="$(shell pwd)/deps/depot_tools:$$PATH" $(depot_tools_config) \
		&& gclient sync
	@touch $@

build/fs-tmp-%/v8/build.ninja: deps/v8-checkout/v8 build/fs-$(build_os_arch)/manifest/gn.pkg \
		$(foreach dep, $(v8_deps), build/fs-%/manifest/$(dep).pkg) \
		$(foreach dep, $(v8_deps_for_build), build/fs-$(build_os_arch)/manifest/$(dep).pkg)
	@$(call print-status,v8,Configuring)
	@$(RM) -r $(@D)
	@mkdir -p $(@D)
	@(set -x \
		&& cd deps/v8-checkout/v8 \
		&& $(xcode_env_setup) \
		&& ../../../build/fs-$(build_os_arch)/bin/gn \
			gen $(shell pwd)/$(@D) \
			--args='$(strip \
				target_os="$(v8_os)" \
				target_cpu="$(v8_cpu)" \
				$(v8_cpu_args) \
				$(v8_buildtype_args) \
				$(v8_platform_args) \
				$(v8_options) \
				$(FRIDA_V8_EXTRA_ARGS) \
			)' \
	) >$(@D)/build.log 2>&1

build/fs-%/manifest/v8.pkg: build/fs-tmp-%/v8/build.ninja
	@$(call print-status,v8,Building)
	@prefix=build/fs-$*; \
	srcdir=deps/v8-checkout/v8; \
	builddir=build/fs-tmp-$*/v8; \
	(set -x \
		&& . build/fs-env-$*.rc \
		&& $(xcode_env_setup) \
		&& ninja -C $$builddir v8_monolith \
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
			-s $$srcdir \
			-b $$builddir \
			-g build/fs-$(build_os_arch)/bin/gn \
			patch $$prefix/include/v8-$(v8_api_version)/v8/v8config.h \
		&& case $* in \
			freebsd-*) \
				libdatadir=libdata; \
				;; \
			*) \
				libdatadir=lib; \
				;; \
		esac \
		&& install -d $$prefix/$$libdatadir/pkgconfig \
		&& ( \
			echo "prefix=\$${frida_sdk_prefix}"; \
			echo "libdir=\$${prefix}/lib"; \
			echo "includedir=\$${prefix}/include/v8-$(v8_api_version)"; \
			echo ""; \
			echo "Name: V8"; \
			echo "Description: V8 JavaScript Engine"; \
			echo "Version: $$($(PYTHON3) releng/v8.py -s $$srcdir get version)"; \
			echo "Libs: -L\$${libdir} -lv8-$(v8_api_version)"; \
			libs=$$($(PYTHON3) releng/v8.py \
				-s $$srcdir \
				-b $$builddir \
				-g build/fs-$(build_os_arch)/bin/gn \
				get libs); \
			[ -n "$$libs" ] && echo "Libs.private: $$libs"; \
			echo "Cflags: -I\$${includedir} -I\$${includedir}/v8" \
		) > $$prefix/$$libdatadir/pkgconfig/v8-$(v8_api_version).pc \
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
		&& . build/fs-env-$*.rc \
		&& $(xcode_env_setup) \
		&& ninja -C $$builddir libc++ \
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
		&& $$($(xcode_run) -f libtool) -static -no_warning_for_no_symbols \
			-o $$prefix/lib/c++/libc++abi.a \
			$$builddir/obj/buildtools/third_party/libc++abi/libc++abi/*.o \
		&& $$($(xcode_run) -f libtool) -static -no_warning_for_no_symbols \
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
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/fs-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=fs \
			FRIDA_ENV_SDK=none \
			FRIDA_TOOLCHAIN_VERSION=$(frida_bootstrap_version) \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done

releng/meson/meson.py:
	git submodule init releng/meson
	git submodule update releng/meson
	@touch $@


.SECONDARY:
