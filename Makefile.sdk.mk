include config.mk

MAKE_J ?= -j 8

repo_base_url := "git://github.com/frida"
repo_suffix := ".git"

zlib_version := 1.2.8
libiconv_version := 1.14
binutils_version := 2.25


build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,mac,')
build_arch := $(shell releng/detect-arch.sh)
build_platform_arch := $(build_platform)-$(build_arch)

ifeq ($(build_platform), linux)
	download := wget -O - -q
else
	download := curl -sS
endif

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

enable_diet := $(shell echo $(host_platform_arch) | egrep -q "^(linux-arm|linux-armhf|linux-mips|linux-mipsel|qnx-.+)$$" && echo 1 || echo 0)


ifeq ($(host_platform), mac)
	iconv := build/fs-%/lib/libiconv.a
endif
ifeq ($(host_platform), ios)
	iconv := build/fs-%/lib/libiconv.a
endif
ifeq ($(host_platform), linux)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	bfd := build/fs-%/lib/libbfd.a
endif
ifeq ($(host_platform), android)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	iconv := build/fs-%/lib/libiconv.a
	bfd := build/fs-%/lib/libbfd.a
endif
ifeq ($(host_platform), qnx)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	iconv := build/fs-%/lib/libiconv.a
	bfd := build/fs-%/lib/libbfd.a
endif
ifeq ($(enable_diet), 0)
	v8 := build/fs-%/lib/pkgconfig/v8.pc
endif


all: build/sdk-$(host_platform)-$(host_arch).tar.bz2
	@echo ""
	@echo "\033[0;32mSuccess!\033[0;39m Here's your SDK: \033[1m$<\033[0m"
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
		build/fs-%/lib/libz.a \
		build/fs-%/lib/pkgconfig/liblzma.pc \
		$(unwind) \
		$(iconv) \
		$(bfd) \
		build/fs-%/lib/pkgconfig/libffi.pc \
		build/fs-%/lib/pkgconfig/glib-2.0.pc \
		build/fs-%/lib/pkgconfig/gee-0.8.pc \
		build/fs-%/lib/pkgconfig/json-glib-1.0.pc \
		$(v8)
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/fs-$* \
		&& [ -d lib32 ] && lib32=lib32 || lib32= \
		&& [ -d lib64 ] && lib64=lib64 || lib64= \
		&& tar -c \
			include \
			lib/*.a \
			lib/*.la \
			lib/glib-2.0 \
			lib/libffi* \
			lib/pkgconfig \
			$$lib32 \
			$$lib64 \
			share/aclocal \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(abspath $(@D)/package) -xf -
	releng/relocatify.sh $(@D)/package $(abspath build/fs-$*)
ifeq ($(host_platform), ios)
	cp $(shell xcrun --sdk macosx --show-sdk-path)/usr/include/mach/mach_vm.h $(@D)/package/include/frida_mach_vm.h
endif
	@touch $@


build/.zlib-stamp:
	$(RM) -r zlib
	mkdir zlib
	$(download) http://zlib.net/zlib-$(zlib_version).tar.gz | tar -C zlib -xz --strip-components 1
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/zlib/Makefile: build/fs-env-%.rc build/.zlib-stamp
	$(RM) -r $(@D)
	mkdir -p build/fs-tmp-$*
	cp -a zlib $(@D)
	. $< \
		&& export PACKAGE_TARNAME=zlib \
		&& . $$CONFIG_SITE \
		&& export CFLAGS CXXFLAGS OBJCFLAGS \
		&& case "$*" in \
			linux-arm) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="arm-linux-gnueabi"; \
				;; \
			linux-armhf) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="arm-linux-gnueabihf"; \
				;; \
			linux-mips) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="mips-linux"; \
				;; \
			linux-mipsel) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="mipsel-linux"; \
				;; \
			android-i386) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="i686-linux-android"; \
				;; \
			android-x86_64) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="x86_64-linux-android"; \
				;; \
			android-arm) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="arm-linux-androideabi"; \
				;; \
			android-arm64) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="aarch64-linux-android"; \
				;; \
			qnx-i386) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="i486-pc-nto-qnx6.6.0"; \
				;; \
			qnx-arm) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="arm-unknown-nto-qnx6.5.0"; \
				;; \
			qnx-armeabi) \
				export PATH="$$(dirname $$NM):$$PATH"; \
				export CHOST="arm-unknown-nto-qnx6.5.0eabi"; \
				;; \
		esac \
		&& cd $(@D) \
		&& ./configure \
			--prefix=$$frida_prefix \
			--static

build/fs-%/lib/libz.a: build/fs-env-%.rc build/fs-tmp-%/zlib/Makefile
	. $< \
		&& cd build/fs-tmp-$*/zlib \
		&& make $(MAKE_J) \
		&& make $(MAKE_J) install
	@touch $@


build/.libiconv-stamp:
	$(RM) -r libiconv
	mkdir libiconv
	cd libiconv \
		&& $(download) http://gnuftp.uib.no/libiconv/libiconv-$(libiconv_version).tar.gz | tar -xz --strip-components 1 \
		&& patch -p1 < ../releng/patches/libiconv-arm64.patch \
		&& patch -p1 < ../releng/patches/libiconv-android.patch
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/libiconv/Makefile: build/fs-env-%.rc build/.libiconv-stamp
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& FRIDA_LEGACY_AUTOTOOLS=1 ../../../libiconv/configure \
			--enable-static \
			--disable-shared \
			--enable-relocatable \
			--disable-rpath

build/fs-%/lib/libiconv.a: build/fs-env-%.rc build/fs-tmp-%/libiconv/Makefile
	. $< \
		&& cd build/fs-tmp-$*/libiconv \
		&& make $(MAKE_J) \
		&& make $(MAKE_J) install
	@touch $@


build/.binutils-stamp:
	$(RM) -r binutils
	mkdir binutils
	cd binutils \
		&& $(download) http://gnuftp.uib.no/binutils/binutils-$(binutils_version).tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/binutils-warnings.patch \
		&& patch -p1 < ../releng/patches/binutils-android.patch \
		&& patch -p1 < ../releng/patches/binutils-qnx.patch
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/binutils/libiberty/Makefile: build/fs-env-%.rc build/.binutils-stamp
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../../binutils/libiberty/configure

build/fs-tmp-%/binutils/bfd/Makefile: build/fs-env-%.rc build/.binutils-stamp
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../../binutils/bfd/configure

build/fs-%/lib/libbfd.a: \
		build/fs-env-%.rc \
		build/fs-%/include/bfd.h \
		build/fs-tmp-%/binutils/libiberty/libiberty.a \
		build/fs-tmp-%/binutils/bfd/libbfd.a
	mkdir -p $(@D)
	$(RM) -r build/fs-tmp-$*/binutils/tmp
	mkdir build/fs-tmp-$*/binutils/tmp
	. $< \
		&& cd build/fs-tmp-$*/binutils/tmp \
		&& $$AR x ../libiberty/libiberty.a \
		&& $$AR x ../bfd/libbfd.a \
		&& $$AR r libbfd-full.a *.o \
		&& $$RANLIB libbfd-full.a \
		&& install -m 644 libbfd-full.a ../../../../$@

build/fs-%/include/bfd.h: build/fs-env-%.rc build/fs-tmp-%/binutils/bfd/Makefile
	. $< && make -C build/fs-tmp-$*/binutils/bfd $(MAKE_J) install-bfdincludeHEADERS

build/fs-tmp-%/binutils/libiberty/libiberty.a: build/fs-env-%.rc build/fs-tmp-%/binutils/libiberty/Makefile
	. $< && make -C $(@D) $(MAKE_J)

build/fs-tmp-%/binutils/bfd/libbfd.a: build/fs-env-%.rc build/fs-tmp-%/binutils/bfd/Makefile
	. $< && make -C $(@D) $(MAKE_J)


define make-git-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone $(repo_base_url)/$1$(repo_suffix)
	@mkdir -p $$(@D)
	@touch $$@

$1/configure: build/fs-env-$(build_platform_arch).rc build/.$1-stamp
	. $$< \
		&& cd $$(@D) \
		&& [ -f autogen.sh ] && NOCONFIGURE=1 ./autogen.sh || autoreconf -ifv

build/fs-tmp-%/$1/Makefile: build/fs-env-%.rc $1/configure $3
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && ../../../$1/configure

$2: build/fs-env-%.rc build/fs-tmp-%/$1/Makefile
	. $$< \
		&& cd build/fs-tmp-$$*/$1 \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums install
	@touch $$@
endef

$(eval $(call make-git-module-rules,xz,build/fs-%/lib/pkgconfig/liblzma.pc,))

$(eval $(call make-git-module-rules,libunwind,build/fs-%/lib/pkgconfig/libunwind.pc,build/fs-%/lib/pkgconfig/liblzma.pc))

$(eval $(call make-git-module-rules,libffi,build/fs-%/lib/pkgconfig/libffi.pc,))

$(eval $(call make-git-module-rules,glib,build/fs-%/lib/pkgconfig/glib-2.0.pc,build/fs-%/lib/pkgconfig/libffi.pc $(iconv) build/fs-%/lib/libz.a))

$(eval $(call make-git-module-rules,libgee,build/fs-%/lib/pkgconfig/gee-0.8.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc))

$(eval $(call make-git-module-rules,json-glib,build/fs-%/lib/pkgconfig/json-glib-1.0.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc))


ifeq ($(host_arch), i386)
	v8_arch := ia32
	android_target_platform := 14
endif
ifeq ($(host_arch), x86_64)
	v8_arch := x64
	android_target_platform := 21
endif
ifeq ($(host_arch), arm)
	v8_arch := arm
	android_target_platform := 14
	v8_abi_flags := -D armfloatabi=softfp
endif
ifeq ($(host_arch), armeabi)
	v8_arch := arm
	android_target_platform := 14
	v8_abi_flags := -D armfloatabi=softfp
endif
ifeq ($(host_arch), armhf)
	v8_arch := arm
	android_target_platform := 14
	v8_abi_flags := -D armfloatabi=hard
endif
ifeq ($(host_arch), arm64)
	v8_arch := arm64
	android_target_platform := 21
endif

ifeq ($(build_platform), mac)
	v8_build_flags := -D clang_xcode=1
endif
ifeq ($(host_platform), linux)
	v8_host_flags := -f make-linux -D clang=0 -D host_clang=0 -D linux_use_bundled_binutils=0 -D linux_use_bundled_gold=0 -D linux_use_gold_flags=0
	v8_libs_private := "-lrt"
endif
ifeq ($(host_platform), android)
	v8_flavor_prefix := android_
	v8_host_flags := -f make-android -D android_ndk_root=$(ANDROID_NDK_ROOT) -D android_sysroot=$(ANDROID_NDK_ROOT) -D android_target_platform=$(android_target_platform) -D clang=1
	v8_libs_private := "-llog -lm"
endif
ifeq ($(host_platform), mac)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.9 -D clang=1
endif
ifeq ($(host_platform), ios)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.9 -D ios_deployment_target=7.0 -D clang=1 -D want_separate_host_toolset=1 -D want_separate_host_toolset_mkpeephole=1
endif
v8_flags := -D host_os=$(build_platform) -D werror='' -D v8_use_external_startup_data=0 -D v8_enable_gdbjit=0 -D v8_enable_i18n_support=0 $(v8_host_flags) $(v8_build_flags) $(v8_abi_flags)

v8_target := $(v8_flavor_prefix)$(v8_arch).release

ifeq ($(build_platform), mac)
ifeq ($(host_platform), mac)
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX -stdlib=libc++" \
		CXX_host="$$CXX -stdlib=libc++" \
		CXX_target="$$CXX -stdlib=libc++" \
		LINK="$$CXX -stdlib=libc++"
endif
ifeq ($(host_platform), ios)
	mac_sdk_path := $$(xcrun --sdk macosx --show-sdk-path)
	v8_env_vars := \
		GYP_CROSSCOMPILE=1 \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX -stdlib=libc++" \
		CXX_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(mac_sdk_path) -stdlib=libc++" \
		CXX_target="$$CXX -stdlib=libc++" \
		LINK="$$CXX -stdlib=libc++" \
		LINK_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(mac_sdk_path) -stdlib=libc++"
endif
ifeq ($(host_platform), android)
	mac_sdk_path := $$(xcrun --sdk macosx --show-sdk-path)
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX" \
		CXX_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(mac_sdk_path) -stdlib=libc++" \
		CXX_target="$$CXX" \
		LINK="$$CXX" \
		LINK_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(mac_sdk_path) -stdlib=libc++" \
		CFLAGS="" \
		CXXFLAGS="" \
		CPPFLAGS="" \
		LDFLAGS=""
endif
else
ifeq ($(build_platform), linux)
ifeq ($(host_platform_arch), linux-arm)
	v8_env_vars := \
		CXX="$$CXX" \
		CXX_host="g++ -m32" \
		CXX_target="$$CXX" \
		LINK="$$CXX" \
		LINK_host="g++ -m32" \
		CFLAGS="$$CFLAGS" \
		LDFLAGS="$$LDFLAGS" \
		CXXFLAGS="$$CXXFLAGS" \
		CPPFLAGS="$$CPPFLAGS"
else
ifeq ($(host_platform_arch), linux-armhf)
	v8_env_vars := \
		CXX="$$CXX" \
		CXX_host="g++ -m32" \
		CXX_target="$$CXX" \
		LINK="$$CXX" \
		LINK_host="g++ -m32" \
		CFLAGS="$$CFLAGS" \
		LDFLAGS="$$LDFLAGS" \
		CXXFLAGS="$$CXXFLAGS" \
		CPPFLAGS="$$CPPFLAGS"
else
	v8_env_vars := \
		CXX_host="$$CXX" \
		CXX_target="$$CXX" \
		LINK="$$CXX"
endif
endif
endif
endif

build/.v8-stamp:
	$(RM) -r v8
	git clone $(repo_base_url)/v8$(repo_suffix)
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/.v8-source-stamp: build/.v8-stamp
	# Poor-man's substitute for out-of-tree builds
	@mkdir -p $(@D)
	$(RM) -r $(@D)/v8
	git clone v8 $(@D)/v8
	@touch $@

build/fs-tmp-%/.v8-build-stamp: build/fs-env-%.rc build/fs-tmp-%/.v8-source-stamp
	. $< \
		&& cd build/fs-tmp-$*/v8 \
		&& PATH="/usr/bin:/bin:/usr/sbin:/sbin:$$PATH" \
			$(v8_env_vars) \
			make $(MAKE_J) $(v8_target) GYPFLAGS="$(v8_flags)"
	@touch $@

build/fs-%/lib/pkgconfig/v8.pc: build/fs-tmp-%/.v8-build-stamp
	install -d build/fs-$*/include/v8/include
	install -m 644 v8/include/*.h build/fs-$*/include/v8/include
	install -d build/fs-$*/include/v8/include/libplatform
	install -m 644 v8/include/libplatform/*.h build/fs-$*/include/v8/include/libplatform
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_libbase.a build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_base.a build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_libplatform.a build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_libsampler.a build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_snapshot.a build/fs-$*/lib
	install -d $(@D)
	echo "prefix=\$${frida_sdk_prefix}" > $@.tmp
	echo "exec_prefix=\$${prefix}" >> $@.tmp
	echo "libdir=\$${exec_prefix}/lib" >> $@.tmp
	echo "includedir=\$${prefix}/include/v8" >> $@.tmp
	echo "" >> $@.tmp
	echo "Name: V8" >> $@.tmp
	echo "Description: V8 JavaScript Engine" >> $@.tmp
	echo "Version: 5.4.401.0" >> $@.tmp
	echo "Libs: -L\$${libdir} -lv8_base -lv8_snapshot -lv8_libplatform -lv8_libsampler -lv8_libbase" >> $@.tmp
ifdef v8_libs_private
	echo Libs.private: $(v8_libs_private) >> $@.tmp
endif
	echo "Cflags: -I\$${includedir} -I\$${includedir}/include" >> $@.tmp
	mv $@.tmp $@


build/fs-env-%.rc:
	FRIDA_HOST=$* \
		FRIDA_OPTIMIZATION_FLAGS="$(FRIDA_OPTIMIZATION_FLAGS)" \
		FRIDA_DEBUG_FLAGS="$(FRIDA_DEBUG_FLAGS)" \
		FRIDA_STRIP=$(FRIDA_STRIP) \
		FRIDA_DIET=$(FRIDA_DIET) \
		FRIDA_MAPPER=$(FRIDA_MAPPER) \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		FRIDA_ENV_NAME=fs \
		FRIDA_ENV_SDK=none \
		./releng/setup-env.sh


.PHONY: all
.SECONDARY:
