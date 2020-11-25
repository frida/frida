include config.mk

MAKE_J ?= -j 8

frida_sdk_version := 20201106
frida_bootstrap_version := 20201028

repo_base_url = https://github.com/frida
repo_suffix := .git

gnu_mirror := saimei.ftp.acc.umu.se/mirror/gnu.org/gnu


libiconv_version := 1.16
libiconv_hash := e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04

elfutils_version := elfutils-0.182

libdwarf_version := 20201020
libdwarf_hash := 1c5ce59e314c6fe74a1f1b4e2fa12caea9c24429309aa0ebdfa882f74f016eff

zlib_version := 91920caec2160ffd919fd48dc4e7a0f6c3fb36d2

xz_version := 6c84113065f603803683d30342207c73465bbc12

sqlite_version := 9f21a054d5c24c2036e9d1b28c630ecda5ae24c3

libunwind_version := 66ca44cd82389cd7cfbbd482e58324a79f6679ab

libffi_version := 4612f7f4b8cfda9a1f07e66d033bb9319860af9b

glib_version := 327fd7518d5612492723aec20c97fd2505e98fd8

glib_networking_version := 7be8c21840cd4eb23477dc0c0d261f85d2c57778

libgee_version := c7e96ac037610cc3d0e11dc964b7b1fca479fc2a

json_glib_version := 9dd3b3898a2c41a1f9af24da8bab22e61526d299

libpsl_version := 3caf6c33029b6c43fc31ce172badf976f6c37bc4

libxml2_version := f1845f6fd1c0b6aac0f573c77a8250f8d4eb31fd

libsoup_version := d0ac83f3952d590da09968b1e30736782b1d1c7f

capstone_version := 03295d19d2c3b0162118a6d9742312301cde1d00

quickjs_version := 26ce42ea32a3318b1c6318d4db6cf01ade54be61

tinycc_version := c9f502d1a14aca5209eae8bdc9de1784ba27cb6f

openssl_version := 1.1.1h
openssl_hash := 5c9ca8774bd7b03e5784f26ae9e9e6d749c9da2438545077e6b3d755a06595d9

v8_version := 6e74fda056e277e0c80240e7a4dd76c7225bd9ec
v8_api_version := 8.0
gn_version := 75194c124f158d7fabdc94048f1a3f850a5f0701
depot_tools_version := b674f8a27725216bd2201652636649d83064ca4a


SHELL := /bin/bash


build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
build_arch := $(shell releng/detect-arch.sh)
build_platform_arch := $(build_platform)-$(build_arch)

ifdef FRIDA_HOST
	host_platform := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
else
	host_platform := $(build_platform)
endif
ifdef FRIDA_HOST
	host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
else
	host_arch := $(build_arch)
endif
host_platform_arch := $(host_platform)-$(host_arch)


ifeq ($(host_platform),$(filter $(host_platform),macos ios))
	iconv := build/fs-%/lib/libiconv.a
ifneq ($(FRIDA_V8), disabled)
ifeq ($(FRIDA_ASAN), no)
	libcxx := build/fs-%/lib/c++/libc++.a
endif
endif
endif
ifeq ($(host_platform), linux)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	elf := build/fs-%/lib/libelf.a
	dwarf := build/fs-%/lib/libdwarf.a
endif
ifeq ($(host_platform), android)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	iconv := build/fs-%/lib/libiconv.a
	elf := build/fs-%/lib/libelf.a
	dwarf := build/fs-%/lib/libdwarf.a
endif
ifeq ($(host_platform), qnx)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	iconv := build/fs-%/lib/libiconv.a
	elf := build/fs-%/lib/libelf.a
	dwarf := build/fs-%/lib/libdwarf.a
endif
ifeq ($(host_platform),$(filter $(host_platform),macos ios linux android))
	glib_tls_provider := build/fs-%/lib/pkgconfig/gioopenssl.pc
endif
ifneq ($(FRIDA_V8), disabled)
	v8 := build/fs-%/lib/pkgconfig/v8-$(v8_api_version).pc
endif

ifeq ($(FRIDA_LIBC), uclibc)
	iconv := build/fs-%/lib/libiconv.a
endif

ifneq ($(iconv),)
	glib_iconv_option := -Diconv=external
endif


all: build/sdk-$(host_platform)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your SDK: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""


build/sdk-$(host_platform)-$(host_arch).tar.bz2: build/fs-tmp-$(host_platform_arch)/.package-stamp
	tar \
		-C build/fs-tmp-$(host_platform_arch)/package \
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
			$$libcpp \
			$$gio_modules \
			$$lib32 \
			$$lib64 \
			share/aclocal \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(abspath $(@D)/package) -xf -
	releng/relocatify.sh $(@D)/package $(abspath build/fs-$*) $(abspath releng)
ifeq ($(host_platform), ios)
	cp $(shell xcrun --sdk macosx --show-sdk-path)/usr/include/mach/mach_vm.h $(@D)/package/include/frida_mach_vm.h
endif
	@touch $@


define download-and-extract
	@$(RM) -r $3
	@mkdir -p $3
	@echo "[*] Downloading $1"
	@if command -v curl >/dev/null; then \
		curl -sSfLo $3/.tarball $1; \
	else \
		wget -qO $3/.tarball $1; \
	fi
	@echo "[*] Verifying checksum"
	@actual_checksum=$$(shasum -a 256 -b $3/.tarball | awk '{ print $$1; }'); \
	case $$actual_checksum in \
		$2) \
			;; \
		*) \
			echo "$1 checksum mismatch, expected=$2, actual=$$actual_checksum"; \
			exit 1; \
			;; \
	esac
	@echo "[*] Extracting to $3"
	@tar -x -f $3/.tarball -z -C $3 --strip-components 1
	@rm $3/.tarball
endef


build/.libiconv-stamp:
	$(call download-and-extract,https://$(gnu_mirror)/libiconv/libiconv-$(libiconv_version).tar.gz,$(libiconv_hash),libiconv)
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/libiconv/Makefile: build/fs-env-%.rc build/.libiconv-stamp
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& ../../../libiconv/configure

build/fs-%/lib/libiconv.a: build/fs-env-%.rc build/fs-tmp-%/libiconv/Makefile
	. $< \
		&& cd build/fs-tmp-$*/libiconv \
		&& make $(MAKE_J) \
		&& make $(MAKE_J) install
	@touch $@


build/.elfutils-stamp: build/fs-env-$(build_platform_arch).rc
	$(RM) -r elfutils
	git clone git://sourceware.org/git/elfutils.git
	. $< \
		&& cd elfutils \
		&& git checkout -b $(frida_sdk_version) $(elfutils_version) \
		&& patch -p1 < ../releng/patches/elfutils-clang.patch \
		&& patch -p1 < ../releng/patches/elfutils-android.patch \
		&& patch -p1 < ../releng/patches/elfutils-glibc-compatibility.patch \
		&& patch -p1 < ../releng/patches/elfutils-musl.patch \
		&& autoreconf -ifv
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/elfutils/Makefile: build/fs-env-%.rc build/.elfutils-stamp build/fs-%/lib/pkgconfig/liblzma.pc build/fs-%/lib/pkgconfig/zlib.pc
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& ../../../elfutils/configure \
			--disable-libdebuginfod \
			--enable-maintainer-mode

build/fs-%/lib/libelf.a: build/fs-env-%.rc build/fs-tmp-%/elfutils/Makefile
	. $< \
		&& cd build/fs-tmp-$*/elfutils \
		&& make $(MAKE_J) -C libelf libelf.a
	install -d build/fs-$*/include
	install -m 644 elfutils/libelf/libelf.h build/fs-$*/include
	install -m 644 elfutils/libelf/elf.h build/fs-$*/include
	install -m 644 elfutils/libelf/gelf.h build/fs-$*/include
	install -m 644 elfutils/libelf/nlist.h build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/elfutils/libelf/libelf.a build/fs-$*/lib
	@touch $@


build/.libdwarf-stamp:
	$(call download-and-extract,https://www.prevanders.net/libdwarf-$(libdwarf_version).tar.gz,$(libdwarf_hash),libdwarf)
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/libdwarf/Makefile: build/fs-env-%.rc build/.libdwarf-stamp build/fs-%/lib/libelf.a
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../libdwarf/configure

build/fs-%/lib/libdwarf.a: build/fs-env-%.rc build/fs-tmp-%/libdwarf/Makefile
	. $< \
		&& make $(MAKE_J) -C build/fs-tmp-$*/libdwarf/libdwarf libdwarf.la
	install -d build/fs-$*/include
	install -m 644 libdwarf/libdwarf/dwarf.h build/fs-$*/include
	install -m 644 libdwarf/libdwarf/libdwarf.h build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/libdwarf/libdwarf/.libs/libdwarf.a build/fs-$*/lib
	@touch $@


define make-git-autotools-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone --recurse-submodules $(repo_base_url)/$1$(repo_suffix)
	cd $1 && git checkout -b $(frida_sdk_version) $2
	@mkdir -p $$(@D)
	@touch $$@

$1/configure: build/fs-env-$(build_platform_arch).rc build/.$1-stamp
	. $$< \
		&& cd $$(@D) \
		&& [ -f autogen.sh ] && NOCONFIGURE=1 ./autogen.sh || autoreconf -ifv

build/fs-tmp-%/$1/Makefile: build/fs-env-%.rc $1/configure $4
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && ../../../$1/configure

$3: build/fs-env-%.rc build/fs-tmp-%/$1/Makefile
	. $$< \
		&& cd build/fs-tmp-$$*/$1 \
		&& make $(MAKE_J) \
		&& make $(MAKE_J) install
	@touch $$@
endef

define make-git-meson-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone --recurse-submodules $(repo_base_url)/$1$(repo_suffix)
	cd $1 && git checkout -b $(frida_sdk_version) $2
	@mkdir -p $$(@D)
	@touch $$@

build/fs-tmp-%/$1/build.ninja: build/fs-env-$(build_platform_arch).rc build/fs-env-%.rc build/.$1-stamp $4 releng/meson/meson.py
	$(RM) -r $$(@D)
	(. build/fs-meson-env-$$*.rc \
		&& . build/fs-config-$$*.site \
		&& $(MESON) \
			--cross-file build/fs-$$*.txt \
			--prefix $$$$frida_prefix \
			--libdir $$$$frida_prefix/lib \
			--default-library static \
			$$(FRIDA_MESONFLAGS_BOTTLE) \
			$5 \
			$$(@D) \
			$1)

$3: build/fs-env-%.rc build/fs-tmp-%/$1/build.ninja
	(. $$< \
		&& $(NINJA) -C build/fs-tmp-$$*/$1 install)
	@touch $$@
endef

$(eval $(call make-git-meson-module-rules,zlib,$(zlib_version),build/fs-%/lib/pkgconfig/zlib.pc,))

$(eval $(call make-git-autotools-module-rules,xz,$(xz_version),build/fs-%/lib/pkgconfig/liblzma.pc,))

$(eval $(call make-git-meson-module-rules,sqlite,$(sqlite_version),build/fs-%/lib/pkgconfig/sqlite3.pc,,))

$(eval $(call make-git-autotools-module-rules,libunwind,$(libunwind_version),build/fs-%/lib/pkgconfig/libunwind.pc,build/fs-%/lib/pkgconfig/liblzma.pc))

$(eval $(call make-git-meson-module-rules,libffi,$(libffi_version),build/fs-%/lib/pkgconfig/libffi.pc,,))

$(eval $(call make-git-meson-module-rules,glib,$(glib_version),build/fs-%/lib/pkgconfig/glib-2.0.pc,$(iconv) build/fs-%/lib/pkgconfig/zlib.pc build/fs-%/lib/pkgconfig/libffi.pc,$(glib_iconv_option) -Dselinux=disabled -Dxattr=false -Dlibmount=disabled -Dinternal_pcre=true -Dtests=false))

$(eval $(call make-git-meson-module-rules,glib-networking,$(glib_networking_version),build/fs-%/lib/pkgconfig/gioopenssl.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc build/fs-%/lib/pkgconfig/openssl.pc,-Dgnutls=disabled -Dopenssl=enabled -Dlibproxy=disabled -Dgnome_proxy=disabled -Dstatic_modules=true))

$(eval $(call make-git-meson-module-rules,libgee,$(libgee_version),build/fs-%/lib/pkgconfig/gee-0.8.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc))

$(eval $(call make-git-meson-module-rules,json-glib,$(json_glib_version),build/fs-%/lib/pkgconfig/json-glib-1.0.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc,-Dintrospection=disabled -Dtests=false))

$(eval $(call make-git-meson-module-rules,libpsl,$(libpsl_version),build/fs-%/lib/pkgconfig/libpsl.pc,,-Dtests=false))

$(eval $(call make-git-meson-module-rules,libxml2,$(libxml2_version),build/fs-%/lib/pkgconfig/libxml-2.0.pc,build/fs-%/lib/pkgconfig/zlib.pc build/fs-%/lib/pkgconfig/liblzma.pc,))

$(eval $(call make-git-meson-module-rules,libsoup,$(libsoup_version),build/fs-%/lib/pkgconfig/libsoup-2.4.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc build/fs-%/lib/pkgconfig/sqlite3.pc build/fs-%/lib/pkgconfig/libpsl.pc build/fs-%/lib/pkgconfig/libxml-2.0.pc,-Dgssapi=disabled -Dtls_check=false -Dgnome=false -Dintrospection=disabled -Dvapi=disabled -Dtests=false -Dsysprof=disabled))

$(eval $(call make-git-meson-module-rules,capstone,$(capstone_version),build/fs-%/lib/pkgconfig/capstone.pc,,-Darchs=$(shell echo $(host_arch) | sed 's,^x86_64$$,x86,') -Dx86_att_disable=true -Dcli=disabled))

$(eval $(call make-git-meson-module-rules,quickjs,$(quickjs_version),build/fs-%/lib/pkgconfig/quickjs.pc,,-Dlibc=false -Dbignum=true -Datomics=disabled -Dstack_check=disabled))

$(eval $(call make-git-meson-module-rules,tinycc,$(tinycc_version),build/fs-%/lib/pkgconfig/libtcc.pc,,))


ifeq ($(FRIDA_ASAN), yes)
	openssl_buildtype_args := \
		enable-asan \
		$(NULL)
else
	openssl_buildtype_args := \
		$(NULL)
endif

ifeq ($(host_platform),$(filter $(host_platform),macos ios))
	xcode_developer_dir := $(shell xcode-select -print-path)
ifeq ($(host_platform_arch), macos-x86)
	openssl_arch_args := macos-i386
	xcode_platform := MacOSX
endif
ifeq ($(host_platform_arch), macos-x86_64)
	openssl_arch_args := macos64-x86_64 enable-ec_nistp_64_gcc_128
	xcode_platform := MacOSX
endif
ifeq ($(host_platform_arch), macos-arm64)
	openssl_arch_args := macos64-cross-arm64 enable-ec_nistp_64_gcc_128
	xcode_platform := MacOSX
endif
ifeq ($(host_platform_arch), macos-arm64e)
	openssl_arch_args := macos64-cross-arm64e enable-ec_nistp_64_gcc_128
	xcode_platform := MacOSX
endif
ifeq ($(host_platform_arch), ios-x86)
	openssl_arch_args := ios-sim-cross-i386
	xcode_platform := iPhoneSimulator
endif
ifeq ($(host_platform_arch), ios-x86_64)
	openssl_arch_args := ios-sim-cross-x86_64 enable-ec_nistp_64_gcc_128
	xcode_platform := iPhoneSimulator
endif
ifeq ($(host_platform_arch), ios-arm)
	openssl_arch_args := ios-cross-armv7 -D__ARM_MAX_ARCH__=7
	xcode_platform := iPhoneOS
endif
ifeq ($(host_platform_arch), ios-arm64)
	openssl_arch_args := ios64-cross-arm64 enable-ec_nistp_64_gcc_128
	xcode_platform := iPhoneOS
endif
ifeq ($(host_platform_arch), ios-arm64e)
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
ifeq ($(host_platform_arch), macos-x86)
ifneq ($(MACOS_X86_SDK_ROOT),)
	openssl_host_env += MACOS_SDK_ROOT="$(MACOS_X86_SDK_ROOT)"
endif
endif
ifeq ($(host_platform_arch),$(filter $(host_platform_arch),macos-arm64 macos-arm64e))
	openssl_host_env += MACOS_MIN_SDK_VERSION=11.0
else
	openssl_host_env += MACOS_MIN_SDK_VERSION=10.9
endif
endif
ifeq ($(host_platform), linux)
ifeq ($(host_arch), x86)
	openssl_arch_args := linux-x86
endif
ifeq ($(host_arch), x86_64)
	openssl_arch_args := linux-x86_64 enable-ec_nistp_64_gcc_128
endif
ifeq ($(host_arch), arm)
	openssl_arch_args := linux-armv4
endif
ifeq ($(host_arch), armhf)
	openssl_arch_args := linux-armv4
endif
ifeq ($(host_arch), armbe8)
	openssl_arch_args := linux-armv4
endif
ifeq ($(host_arch), arm64)
	openssl_arch_args := linux-aarch64
endif
ifeq ($(host_arch), mipsel)
	openssl_arch_args := linux-mips32
endif
ifeq ($(host_arch), mips)
	openssl_arch_args := linux-mips32
endif
ifeq ($(host_arch), mips64el)
	openssl_arch_args := linux-mips64
endif
ifeq ($(host_arch), mips64)
	openssl_arch_args := linux-mips64
endif
	openssl_host_env := \
		$(NULL)
endif
ifeq ($(host_platform), android)
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
	ndk_build_platform_arch := $(shell uname -s | tr '[A-Z]' '[a-z]')-$(build_arch)
	ndk_llvm_prefix := $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ndk_build_platform_arch)
	ndk_gcc_prefix := $(ANDROID_NDK_ROOT)/toolchains/$(ndk_abi)-4.9/prebuilt/$(ndk_build_platform_arch)
	openssl_host_env := \
		CPP=clang CC=clang CXX=clang++ LD= LDFLAGS= AR=$(ndk_triplet)-ar RANLIB=$(ndk_triplet)-ranlib \
		ANDROID_NDK=$(ANDROID_NDK_ROOT) \
		PATH=$(ndk_llvm_prefix)/bin:$(ndk_gcc_prefix)/bin:$$PATH \
		$(NULL)
endif

build/.openssl-stamp:
	$(call download-and-extract,https://www.openssl.org/source/openssl-$(openssl_version).tar.gz,$(openssl_hash),openssl)
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/openssl/Configure: build/.openssl-stamp
	$(RM) -r $(@D)
	mkdir -p build/fs-tmp-$*
	cp -a openssl $(@D)
	@touch $@

build/fs-%/lib/pkgconfig/openssl.pc: build/fs-env-%.rc build/fs-tmp-%/openssl/Configure
	. $< \
		&& . $$CONFIG_SITE \
		&& export CC CFLAGS \
		&& export $(openssl_host_env) OPENSSL_LOCAL_CONFIG_DIR="$(abspath releng/openssl-config)" \
		&& cd build/fs-tmp-$*/openssl \
		&& perl Configure \
			--prefix=$$frida_prefix \
			--openssldir=/etc/ssl \
			no-engine \
			no-tests \
			no-comp \
			no-ssl3 \
			no-zlib \
			no-async \
			no-shared \
			enable-cms \
			$(openssl_buildtype_args) \
			$(openssl_arch_args) \
		&& make depend \
		&& make build_libs \
		&& make install_dev


v8_common_args := \
	use_thin_lto=false \
	v8_monolithic=true \
	v8_use_external_startup_data=false \
	is_component_build=false \
	v8_enable_debugging_features=false \
	v8_enable_disassembler=false \
	v8_enable_gdbjit=false \
	v8_enable_i18n_support=false \
	v8_untrusted_code_mitigations=false \
	treat_warnings_as_errors=false \
	fatal_linker_warnings=false \
	use_glib=false \
	use_goma=false \
	v8_embedder_string="-frida" \
	$(NULL)

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
ifeq ($(host_arch),$(filter $(host_arch),arm armbe8 armeabi armhf))
	v8_cpu := arm
	v8_cpu_args := arm_version=7 arm_fpu="vfpv3-d16" arm_use_neon=false
ifneq ($(host_platform), android)
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

v8_build_platform := $(shell echo $(build_platform) | sed 's,^macos$$,mac,')
ifeq ($(host_platform), macos)
	v8_os := mac
	v8_platform_args := $(NULL)
ifeq ($(host_arch),$(filter $(host_arch),arm64 arm64e))
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
ifeq ($(host_platform), ios)
	v8_os := ios
	v8_platform_args := \
		use_xcode_clang=true \
		mac_deployment_target="10.9" \
		ios_deployment_target="8.0" \
		$(NULL)
endif
ifeq ($(host_platform),$(filter $(host_platform),macos ios))
ifeq ($(host_arch),$(filter $(host_arch),arm64 arm64e))
	v8_platform_args += v8_enable_pointer_compression=false
endif
endif
ifeq ($(host_platform), linux)
	v8_os := linux
	v8_platform_args := \
		is_clang=false \
		is_cfi=false \
		use_sysroot=false \
		linux_use_bundled_binutils=false \
		use_gold=false
	v8_libs_private := "-lrt"
endif
ifeq ($(host_platform), android)
	v8_os := android
	v8_platform_args := \
		use_xcode_clang=true \
		use_custom_libcxx=false \
		android_ndk_root="$(ANDROID_NDK_ROOT)" \
		android_ndk_version="r21" \
		android_ndk_major_version=21 \
		android32_ndk_api_level=18 \
		android64_ndk_api_level=21 \
		clang_base_path="$(abspath $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ndk_build_platform_arch))"
	v8_libs_private := "-llog -lm"
endif

ifeq ($(host_platform),$(filter $(host_platform),linux android))
	v8_platform_args += enable_resource_allowlist_generation=false
endif

ifeq ($(host_arch),$(filter $(host_arch),x86 arm))
ifneq ($(MACOS_X86_SDK_ROOT),)
	v8_platform_args += mac_sdk_path="$(MACOS_X86_SDK_ROOT)"
endif
endif
ifneq ($(IOS_SDK_ROOT),)
	v8_platform_args += ios_sdk_path="$(IOS_SDK_ROOT)"
endif

gn:
	# Google's prebuilt GN requires a newer glibc than our Debian Squeeze buildroot has.
	git clone $(repo_base_url)/gn$(repo_suffix)
	cd gn && git checkout -b $(frida_sdk_version) $(gn_version)

build/fs-tmp-%/gn/build.ninja: build/fs-env-%.rc gn
	. $< \
		&& CC="$$CC" CXX="$$CXX" python gn/build/gen.py \
			--out-path $(abspath $(@D))

build/fs-tmp-%/gn/gn: build/fs-tmp-%/gn/build.ninja
	$(NINJA) -C build/fs-tmp-$*/gn
	@touch $@

v8-checkout/depot_tools/gclient:
	$(RM) -r v8-checkout/depot_tools
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git v8-checkout/depot_tools
	cd v8-checkout/depot_tools && git checkout -b $(frida_sdk_version) $(depot_tools_version)

v8-checkout/.gclient: v8-checkout/depot_tools/gclient
	cd v8-checkout && PATH=$(abspath v8-checkout/depot_tools):$$PATH depot_tools/gclient config --spec 'solutions = [ \
  { \
    "url": "$(repo_base_url)/v8.git@$(v8_version)", \
    "managed": False, \
    "name": "v8", \
    "deps_file": "DEPS", \
    "custom_deps": {}, \
  }, \
]'

v8-checkout/v8: v8-checkout/.gclient
	cd v8-checkout && PATH=$(abspath v8-checkout/depot_tools):$$PATH gclient sync
	@touch $@

build/fs-tmp-%/v8/build.ninja: v8-checkout/v8 build/fs-tmp-$(build_platform_arch)/gn/gn
	cd v8-checkout/v8 \
		&& ../../build/fs-tmp-$(build_platform_arch)/gn/gn \
			gen $(abspath $(@D)) \
			--args='target_os="$(v8_os)" target_cpu="$(v8_cpu)" $(v8_cpu_args) $(v8_common_args) $(v8_buildtype_args) $(v8_platform_args)'

build/fs-tmp-%/v8/obj/libv8_monolith.a: build/fs-tmp-%/v8/build.ninja
	$(NINJA) -C build/fs-tmp-$*/v8 v8_monolith
	@touch $@

build/fs-%/lib/pkgconfig/v8-$(v8_api_version).pc: build/fs-tmp-%/v8/obj/libv8_monolith.a
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8
	install -m 644 v8-checkout/v8/include/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/inspector
	install -m 644 build/fs-tmp-$*/v8/gen/include/inspector/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/inspector/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/libplatform
	install -m 644 v8-checkout/v8/include/libplatform/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/libplatform/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc
	install -m 644 v8-checkout/v8/include/cppgc/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/
	install -d build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/internal
	install -m 644 v8-checkout/v8/include/cppgc/internal/*.h build/fs-$*/include/v8-$(v8_api_version)/v8/cppgc/internal/
	install -d build/fs-$*/lib
	install -m 644 $< build/fs-$*/lib/libv8-$(v8_api_version).a
	install -d $(@D)
	$(PYTHON3) releng/v8.py \
		patch build/fs-$*/include/v8-$(v8_api_version)/v8/v8config.h \
		-s v8-checkout/v8 \
		-b build/fs-tmp-$*/v8 \
		-G build/fs-tmp-$(build_platform_arch)/gn/gn
	echo "prefix=\$${frida_sdk_prefix}" > $@.tmp
	echo "libdir=\$${prefix}/lib" >> $@.tmp
	echo "includedir=\$${prefix}/include/v8-$(v8_api_version)" >> $@.tmp
	echo "" >> $@.tmp
	echo "Name: V8" >> $@.tmp
	echo "Description: V8 JavaScript Engine" >> $@.tmp
	echo "Version: $$($(PYTHON3) releng/v8.py get version -s v8-checkout/v8)" >> $@.tmp
	echo "Libs: -L\$${libdir} -lv8-$(v8_api_version)" >> $@.tmp
ifdef v8_libs_private
	echo Libs.private: $(v8_libs_private) >> $@.tmp
endif
	echo "Cflags: -I\$${includedir} -I\$${includedir}/v8" >> $@.tmp
	mv $@.tmp $@


build/fs-%/lib/c++/libc++.a: build/fs-tmp-%/v8/obj/libv8_monolith.a
	$(NINJA) -C build/fs-tmp-$*/v8 libc++
	install -d build/fs-$*/include/c++/
	cp -a v8-checkout/v8/buildtools/third_party/libc++/trunk/include/* build/fs-$*/include/c++/
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
		v8-checkout/v8/buildtools/third_party/libc++/trunk/include/__config \
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


.PHONY: all
.SECONDARY:
