include config.mk

MAKE_J ?= -j 8

repo_base_url := "git://github.com/frida"
repo_suffix := ".git"

zlib_version := 1.2.11
libiconv_version := 1.14
elfutils_version := 0.170
libdwarf_version := 20170709
openssl_version := 1.1.0h


build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
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

enable_diet := $(shell echo $(host_platform_arch) | egrep -q "^(linux-arm|linux-mips|linux-mipsel|qnx-.+)$$" && echo 1 || echo 0)


ifeq ($(host_platform), macos)
	iconv := build/fs-%/lib/libiconv.a
	glib_tls_provider := build/fs-%/lib/pkgconfig/glib-openssl-static.pc
	glib_tls_args := -Dca_certificates=no
ifeq ($(host_arch), x86)
	openssl_arch_args := darwin-i386-cc
endif
ifeq ($(host_arch), x86_64)
	openssl_arch_args := darwin64-x86_64-cc enable-ec_nistp_64_gcc_128
endif
endif
ifeq ($(host_platform), ios)
	iconv := build/fs-%/lib/libiconv.a
endif
ifeq ($(host_platform), linux)
	unwind := build/fs-%/lib/pkgconfig/libunwind.pc
	elf := build/fs-%/lib/libelf.a
	dwarf := build/fs-%/lib/libdwarf.a
	glib_tls_provider := build/fs-%/lib/pkgconfig/glib-openssl-static.pc
	glib_tls_args := -Dca_certificates=no
ifeq ($(host_arch), x86)
	openssl_arch_args := linux-x86
endif
ifeq ($(host_arch), x86_64)
	openssl_arch_args := linux-x86_64
endif
ifeq ($(host_arch), arm)
	openssl_arch_args := linux-armv4
endif
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
ifeq ($(enable_diet), 0)
	v8 := build/fs-%/lib/pkgconfig/v8.pc
endif


all: build/sdk-$(host_platform)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\033[0;32mSuccess!\033[0;39m Here's your SDK: \033[1m$<\033[0m"
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
		$(elf) \
		$(dwarf) \
		build/fs-%/lib/pkgconfig/libffi.pc \
		build/fs-%/lib/pkgconfig/glib-2.0.pc \
		$(glib_tls_provider) \
		build/fs-%/lib/pkgconfig/gee-0.8.pc \
		build/fs-%/lib/pkgconfig/json-glib-1.0.pc \
		$(v8)
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/fs-$* \
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
			$$gio_modules \
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
		&& export CC CFLAGS \
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
			android-x86) \
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
			qnx-x86) \
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


build/.elfutils-stamp:
	$(RM) -r elfutils
	mkdir elfutils
	cd elfutils \
		&& $(download) https://sourceware.org/pub/elfutils/$(elfutils_version)/elfutils-$(elfutils_version).tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/elfutils-android.patch
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/elfutils/Makefile: build/fs-env-%.rc build/.elfutils-stamp build/fs-%/lib/pkgconfig/liblzma.pc
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< \
		&& cd $(@D) \
		&& if [ -n "$$FRIDA_GCC" ]; then \
			export CC="$$FRIDA_GCC"; \
		fi \
		&& ../../../elfutils/configure

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
	$(RM) -r libdwarf
	mkdir libdwarf
	cd libdwarf \
		&& $(download) https://www.prevanders.net/libdwarf-$(libdwarf_version).tar.gz | tar -xz --strip-components 1
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/libdwarf/Makefile: build/fs-env-%.rc build/.libdwarf-stamp build/fs-%/lib/libelf.a
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../libdwarf/libdwarf/configure

build/fs-%/lib/libdwarf.a: build/fs-env-%.rc build/fs-tmp-%/libdwarf/Makefile
	. $< \
		&& cd build/fs-tmp-$*/libdwarf \
		&& make $(MAKE_J) HOSTCC="gcc" HOSTCFLAGS="" HOSTLDFLAGS="" libdwarf.a
	install -d build/fs-$*/include
	install -m 644 libdwarf/libdwarf/dwarf.h build/fs-$*/include
	install -m 644 build/fs-tmp-$*/libdwarf/libdwarf.h build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/libdwarf/libdwarf.a build/fs-$*/lib
	@touch $@


define make-git-autotools-module-rules
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
	. $$< && cd $$(@D) && VALAFLAGS="$$$$VALAFLAGS --target-glib=2.53" ../../../$1/configure

$2: build/fs-env-%.rc build/fs-tmp-%/$1/Makefile
	. $$< \
		&& cd build/fs-tmp-$$*/$1 \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums install
	@touch $$@
endef

define make-git-meson-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone $(repo_base_url)/$1$(repo_suffix)
	@mkdir -p $$(@D)
	@touch $$@

build/fs-tmp-%/$1/build.ninja: build/fs-env-$(build_platform_arch).rc build/fs-env-%.rc build/.$1-stamp $3 releng/meson/meson.py
	$(RM) -r $$(@D)
	(. build/fs-meson-env-$(build_platform_arch).rc \
		&& . build/fs-config-$$*.site \
		&& $(MESON) \
			--prefix $$$$frida_prefix \
			--libdir $$$$frida_prefix/lib \
			--default-library static \
			--buildtype minsize \
			--cross-file build/fs-$$*.txt \
			$4 \
			$$(@D) \
			$1)

$2: build/fs-env-%.rc build/fs-tmp-%/$1/build.ninja
	(. $$< \
		&& ninja -C build/fs-tmp-$$*/$1 install)
	@touch $$@
endef

$(eval $(call make-git-autotools-module-rules,xz,build/fs-%/lib/pkgconfig/liblzma.pc,))

$(eval $(call make-git-autotools-module-rules,libunwind,build/fs-%/lib/pkgconfig/libunwind.pc,build/fs-%/lib/pkgconfig/liblzma.pc))

$(eval $(call make-git-autotools-module-rules,libffi,build/fs-%/lib/pkgconfig/libffi.pc,))

$(eval $(call make-git-autotools-module-rules,glib,build/fs-%/lib/pkgconfig/glib-2.0.pc,build/fs-%/lib/pkgconfig/libffi.pc $(iconv) build/fs-%/lib/libz.a))

build/.openssl-stamp:
	$(RM) -r openssl
	mkdir openssl
	$(download) https://www.openssl.org/source/openssl-$(openssl_version).tar.gz | tar -C openssl -xz --strip-components 1
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
		&& cd build/fs-tmp-$*/openssl \
		&& perl Configure \
			--prefix=$$frida_prefix \
			--openssldir=/etc/ssl \
			no-comp \
			no-ssl2 \
			no-ssl3 \
			no-zlib \
			no-shared \
			enable-cms \
			$(openssl_arch_args) \
		&& make depend \
		&& make \
		&& make install_sw

$(eval $(call make-git-meson-module-rules,glib-openssl,build/fs-%/lib/pkgconfig/glib-openssl-static.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc build/fs-%/lib/pkgconfig/openssl.pc,$(glib_tls_args)))

$(eval $(call make-git-autotools-module-rules,libgee,build/fs-%/lib/pkgconfig/gee-0.8.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc))

$(eval $(call make-git-meson-module-rules,json-glib,build/fs-%/lib/pkgconfig/json-glib-1.0.pc,build/fs-%/lib/pkgconfig/glib-2.0.pc,-Dintrospection=false))


ifeq ($(host_arch), x86)
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

v8_build_platform := $(shell echo $(build_platform) | sed 's,^macos$$,mac,')
ifeq ($(build_platform), macos)
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
ifeq ($(host_platform), macos)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.9 -D clang=1
endif
ifeq ($(host_platform), ios)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.9 -D ios_deployment_target=7.0 -D clang=1 -D want_separate_host_toolset=1
endif
v8_flags := -D host_os=$(v8_build_platform) -D werror='' -D v8_use_external_startup_data=0 -D v8_enable_gdbjit=0 -D v8_enable_i18n_support=0 -D v8_enable_inspector=1 $(v8_host_flags) $(v8_build_flags) $(v8_abi_flags)

v8_target := $(v8_flavor_prefix)$(v8_arch).release

ifeq ($(build_platform), macos)
ifeq ($(host_platform), macos)
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX -stdlib=libc++" \
		CXX_host="$$CXX -stdlib=libc++" \
		CXX_target="$$CXX -stdlib=libc++" \
		LINK="$$CXX -stdlib=libc++"
endif
ifeq ($(host_platform), ios)
	macos_sdk_path := $$(xcrun --sdk macosx --show-sdk-path)
	v8_env_vars := \
		GYP_CROSSCOMPILE=1 \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX -stdlib=libc++" \
		CXX_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(macos_sdk_path) -stdlib=libc++" \
		CXX_target="$$CXX -stdlib=libc++" \
		LINK="$$CXX -stdlib=libc++" \
		LINK_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(macos_sdk_path) -stdlib=libc++"
endif
ifeq ($(host_platform), android)
	macos_sdk_path := $$(xcrun --sdk macosx --show-sdk-path)
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX" \
		CXX_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(macos_sdk_path) -stdlib=libc++" \
		CXX_target="$$CXX" \
		LINK="$$CXX" \
		LINK_host="$$(xcrun --sdk macosx -f clang++) -isysroot $(macos_sdk_path) -stdlib=libc++" \
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
	if test -f /usr/bin/python2.7; then \
		ln -sf /usr/bin/python2.7 $(@D)/python; \
	else \
		ln -sf /usr/bin/python2.6 $(@D)/python; \
	fi
	. $< \
		&& cd build/fs-tmp-$*/v8 \
		&& PATH="$(abspath $(@D)):/usr/bin:/bin:/usr/sbin:/sbin:$$PATH" \
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
	echo "Version: 6.2.2.0" >> $@.tmp
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
		FRIDA_ASAN=$(FRIDA_ASAN) \
		FRIDA_ENV_NAME=fs \
		FRIDA_ENV_SDK=none \
		./releng/setup-env.sh

releng/meson/meson.py:
	git submodule init releng/meson
	git submodule update releng/meson
	@touch $@


.PHONY: all
.SECONDARY:
