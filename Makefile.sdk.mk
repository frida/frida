MAKE_J ?= -j 8
REPO_BASE_URL = "git://github.com/frida"
REPO_SUFFIX = ".git"

build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,mac,')
build_arch := $(shell uname -m)
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
	host_arch := $(shell uname -m)
endif
host_platform_arch := $(host_platform)-$(host_arch)


ifeq ($(host_platform), linux)
	bfd := build/fs-%/lib/libbfd.a
endif
ifeq ($(host_platform), android)
	iconv := build/fs-%/lib/libiconv.a
	bfd := build/fs-%/lib/libbfd.a
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
		$(iconv) \
		$(bfd) \
		build/fs-%/lib/pkgconfig/libffi.pc \
		build/fs-%/lib/pkgconfig/glib-2.0.pc \
		build/fs-%/lib/pkgconfig/gee-1.0.pc \
		build/fs-%/lib/pkgconfig/json-glib-1.0.pc \
		build/fs-%/lib/pkgconfig/v8.pc
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/fs-$* \
		&& tar -c \
			include \
			lib/*.a \
			lib/*.la \
			lib/glib-2.0 \
			lib/libffi* \
			lib/pkgconfig \
			share/aclocal \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(abspath $(@D)/package) -xf -
	releng/relocatify.sh $(@D)/package $(abspath build/fs-$*) $(abspath build/fs-sdk-$*)
ifeq ($(host_platform), ios)
	cp /System/Library/Frameworks/Kernel.framework/Versions/A/Headers/mach/mach_vm.h $(@D)/package/include/frida_mach_vm.h
endif
	@touch $@


build/.libiconv-stamp:
	$(RM) -r libiconv
	mkdir libiconv
	cd libiconv \
		&& $(download) http://gnuftp.uib.no/libiconv/libiconv-1.14.tar.gz | tar -xz --strip-components 1 \
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
			--enable-relocatable \
			--disable-rpath

build/fs-%/lib/libiconv.a: build/fs-env-%.rc build/fs-tmp-%/libiconv/Makefile
	. $< && make -C build/fs-tmp-$*/libiconv $(MAKE_J)
	touch $@


build/.binutils-stamp:
	$(RM) -r binutils
	mkdir binutils
	cd binutils \
		&& $(download) http://gnuftp.uib.no/binutils/binutils-2.24.tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/binutils-android.patch
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


define make-plain-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone $(REPO_BASE_URL)/$1$(REPO_SUFFIX)
	@mkdir -p $$(@D)
	@touch $$@

$1/configure: build/fs-env-$(build_platform_arch).rc build/.$1-stamp
	. $$< && cd $$(@D) && NOCONFIGURE=1 ./autogen.sh

build/fs-tmp-%/$1/Makefile: build/fs-env-%.rc $1/configure
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && ../../../$1/configure

build/fs-%/lib/pkgconfig/$2.pc: build/fs-env-%.rc build/fs-tmp-%/$1/Makefile $3
	. $$< && make -C build/fs-tmp-$$*/$1 $(MAKE_J) install GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums
	@touch $$@
endef

$(eval $(call make-plain-module-rules,libffi,libffi,))
$(eval $(call make-plain-module-rules,glib,glib-2.0,$(iconv)))
$(eval $(call make-plain-module-rules,libgee,gee-1.0,build/fs-%/lib/pkgconfig/glib-2.0.pc))
$(eval $(call make-plain-module-rules,json-glib,json-glib-1.0,build/fs-%/lib/pkgconfig/glib-2.0.pc))


ifeq ($(host_arch), i386)
	v8_arch := ia32
endif
ifeq ($(host_arch), x86_64)
	v8_arch := x64
endif
ifeq ($(host_arch), arm)
	v8_arch := arm
endif
ifeq ($(host_arch), arm64)
	v8_arch := arm64
endif

ifeq ($(host_platform), linux)
	v8_host_flags := -f make-linux
endif
ifeq ($(host_platform), android)
	v8_flavor_prefix := android_
	v8_host_flags := -f make-android -D clang=1
endif
ifeq ($(host_platform), mac)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.7 -D clang=1
endif
ifeq ($(host_platform), ios)
	v8_host_flags := -f make-mac -D mac_deployment_target=10.7 -D ios_deployment_target=7.0 -D clang=1
endif
v8_flags := -D host_os=$(build_platform) -D werror='' -Dv8_enable_gdbjit=0 -Dv8_enable_i18n_support=0 $(v8_host_flags)

v8_target := $(v8_flavor_prefix)$(v8_arch).release

ifeq ($(build_platform), mac)
ifeq ($(host_platform), android)
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX" \
		CXX_host="$$(xcrun --sdk macosx10.9 -f clang++) -stdlib=libc++" \
		CXX_target="$$CXX" \
		LINK="$$CXX" \
		LINK_host="$$(xcrun --sdk macosx10.9 -f clang++) -stdlib=libc++" \
		CFLAGS="" \
		CXXFLAGS="" \
		CPPFLAGS="" \
		LDFLAGS=""
else
	v8_env_vars := \
		MACOSX_DEPLOYMENT_TARGET="" \
		CXX="$$CXX -stdlib=libc++" \
		CXX_host="$$CXX -stdlib=libc++" \
		CXX_target="$$CXX -stdlib=libc++" \
		LINK="$$CXX -stdlib=libc++"
endif
else
	v8_env_vars := \
		CXX_host="$$CXX" \
		CXX_target="$$CXX" \
		LINK="$$CXX"
endif

build/.v8-stamp:
	$(RM) -r v8
	git clone $(REPO_BASE_URL)/v8$(REPO_SUFFIX)
	@mkdir -p $(@D)
	@touch $@

build/fs-tmp-%/.v8-stamp: build/.v8-stamp
	# Poor-man's substitute for out-of-tree builds
	@mkdir -p $(@D)
	git clone --depth 1 v8 $(@D)/v8
	@touch $@

build/fs-tmp-%/v8/out/$(v8_target)/libv8_base.$(v8_arch).a: build/fs-env-%.rc build/fs-tmp-%/.v8-stamp
	. $< \
		&& cd build/fs-tmp-$*/v8 \
		&& PATH="/usr/bin:/bin:/usr/sbin:/sbin" \
			$(v8_env_vars) \
			make $(MAKE_J) $(v8_target) GYPFLAGS="$(v8_flags)"
	@touch $@

build/fs-%/lib/pkgconfig/v8.pc: build/fs-tmp-%/v8/out/$(v8_target)/libv8_base.$(v8_arch).a
	install -d build/fs-$*/include
	install -m 644 v8/include/* build/fs-$*/include
	install -d build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_base.$(v8_arch).a build/fs-$*/lib
	install -m 644 build/fs-tmp-$*/v8/out/$(v8_target)/libv8_snapshot.a build/fs-$*/lib
	install -d $(@D)
	echo "prefix=\$${frida_sdk_prefix}" > $@.tmp
	echo "exec_prefix=\$${prefix}" >> $@.tmp
	echo "libdir=\$${exec_prefix}/lib" >> $@.tmp
	echo "includedir=\$${prefix}/include" >> $@.tmp
	echo "" >> $@.tmp
	echo "Name: V8" >> $@.tmp
	echo "Description: V8 JavaScript Engine" >> $@.tmp
	echo "Version: 3.26.6.1" >> $@.tmp
	echo "Libs: -L\$${libdir} -lv8_base.$(v8_arch) -lv8_snapshot" >> $@.tmp
	echo "Cflags: -I\$${includedir}" >> $@.tmp
	mv $@.tmp $@


build/.fs-sdk-stamp:
	$(RM) -r build/fs-sdk-$(host_platform_arch)
	mkdir -p build/fs-sdk-$(host_platform_arch)/share/aclocal
	touch build/fs-sdk-$(host_platform_arch)/.stamp
	touch $@

build/fs-env-%.rc: build/.fs-sdk-stamp
	FRIDA_ENV_NAME=fs FRIDA_HOST=$* ./releng/setup-env.sh


.PHONY: all
.SECONDARY:
