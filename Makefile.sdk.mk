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
	host_platform := $(shell echo -n $(FRIDA_HOST) | sed 's,\([a-z]\+\)-\(.\+\),\1,g')
else
	host_platform := $(build_platform)
endif
ifeq ($(host_platform), linux)
	host_distro := $(shell lsb_release -is | tr '[A-Z]' '[a-z]')_$(shell lsb_release -cs)
else
	host_distro := all
endif
ifdef FRIDA_HOST
	host_arch := $(shell echo -n $(FRIDA_HOST) | sed 's,\([a-z]\+\)-\(.\+\),\2,g')
else
	host_arch := $(shell uname -m)
endif
host_platform_arch := $(host_platform)-$(host_arch)


ifeq ($(host_platform), linux)
	bfd := build/frida-%/lib/libbfd.a
endif
ifeq ($(host_platform), android)
	iconv := build/frida-%/lib/libiconv.a
	bfd := build/frida-%/lib/libbfd.a
endif


all: build/tmp-$(host_platform_arch)/sdk-package-stamp


build/tmp-%/sdk-package-stamp: \
		$(iconv) \
		$(bfd) \
		build/frida-%/lib/pkgconfig/libffi.pc \
		build/frida-%/lib/pkgconfig/glib-2.0.pc \
		build/frida-%/lib/pkgconfig/gee-1.0.pc \
		build/frida-%/lib/pkgconfig/json-glib-1.0.pc \
		build/frida-%/lib/pkgconfig/v8.pc


build/binutils-stamp: build/.clean-sdk-stamp
	$(RM) -rf binutils
	mkdir binutils
	cd binutils \
		&& $(download) http://gnuftp.uib.no/binutils/binutils-2.24.tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/binutils-android.patch
	@mkdir -p $(@D)
	@touch $@

build/tmp-%/binutils/libiberty/Makefile: build/frida-env-%.rc build/binutils-stamp
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../../binutils/libiberty/configure

build/tmp-%/binutils/bfd/Makefile: build/frida-env-%.rc build/binutils-stamp
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && ../../../../binutils/bfd/configure

build/frida-%/lib/libbfd.a: \
		build/frida-env-%.rc \
		build/frida-%/include/bfd.h \
		build/tmp-%/binutils/libiberty/libiberty.a \
		build/tmp-%/binutils/bfd/libbfd.a
	mkdir -p $(@D)
	rm -rf build/tmp-$*/binutils/tmp
	mkdir build/tmp-$*/binutils/tmp
	. $< \
		&& cd build/tmp-$*/binutils/tmp \
		&& $$AR x ../libiberty/libiberty.a \
		&& $$AR x ../bfd/libbfd.a \
		&& $$AR r libbfd-full.a *.o \
		&& $$RANLIB libbfd-full.a \
		&& install -m 644 libbfd-full.a ../../../../$@

build/frida-%/include/bfd.h: build/frida-env-%.rc build/tmp-%/binutils/bfd/Makefile
	. $< && make -C build/tmp-$*/binutils/bfd $(MAKE_J) install-bfdincludeHEADERS

build/tmp-%/binutils/libiberty/libiberty.a: build/frida-env-%.rc build/tmp-%/binutils/libiberty/Makefile
	. $< && make -C $(@D) $(MAKE_J)

build/tmp-%/binutils/bfd/libbfd.a: build/frida-env-%.rc build/tmp-%/binutils/bfd/Makefile
	. $< && make -C $(@D) $(MAKE_J)


define make-plain-module-rules
build/$1-stamp: build/.clean-sdk-stamp
	$(RM) -rf $1
	git clone $(REPO_BASE_URL)/$1$(REPO_SUFFIX)
	@mkdir -p $$(@D)
	@touch $$@

$1/configure: build/frida-env-$(build_platform_arch).rc build/$1-stamp
	. $$< && cd $$(@D) && NOCONFIGURE=1 ./autogen.sh

build/tmp-%/$1/Makefile: build/frida-env-%.rc $1/configure
	$(RM) -rf $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && ../../../$1/configure

build/frida-%/lib/pkgconfig/$2.pc: build/frida-env-%.rc build/tmp-%/$1/Makefile $3
	. $$< && make -C build/tmp-$$*/$1 $(MAKE_J) install GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums
	@touch $$@
endef

$(eval $(call make-plain-module-rules,libffi,libffi,))
$(eval $(call make-plain-module-rules,glib,glib-2.0,))
$(eval $(call make-plain-module-rules,libgee,gee-1.0,build/frida-%/lib/pkgconfig/glib-2.0.pc))
$(eval $(call make-plain-module-rules,json-glib,json-glib-1.0,build/frida-%/lib/pkgconfig/glib-2.0.pc))


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

build/v8-stamp: build/.clean-sdk-stamp
	$(RM) -rf v8
	git clone $(REPO_BASE_URL)/v8$(REPO_SUFFIX)
	@mkdir -p $(@D)
	@touch $@

build/tmp-%/v8-stamp: build/v8-stamp
	# Poor-man's substitute for out-of-tree builds
	@mkdir -p $(@D)
	git clone --depth 1 v8 $(@D)/v8
	@touch $@

build/tmp-%/v8/out/$(v8_target)/libv8_base.$(v8_arch).a: build/frida-env-%.rc build/tmp-%/v8-stamp
	. $< \
		&& cd build/tmp-$*/v8 \
		&& PATH="/usr/bin:/bin:/usr/sbin:/sbin" \
			$(v8_env_vars) \
			make $(v8_target) GYPFLAGS="$(v8_flags)" V=1
	@touch $@

build/frida-%/lib/pkgconfig/v8.pc: build/tmp-%/v8/out/$(v8_target)/libv8_base.$(v8_arch).a
	install -d build/frida-$*/include
	install -m 644 v8/include/* build/frida-$*/include
	install -d build/frida-$*/lib
	install -m 644 build/tmp-$*/v8/out/$(v8_target)/libv8_base.$(v8_arch).a build/frida-$*/lib
	install -m 644 build/tmp-$*/v8/out/$(v8_target)/libv8_snapshot.a build/frida-$*/lib
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


build/.clean-sdk-stamp:
	rm -rf build
	mkdir -p build/sdk-$(host_platform_arch)/share/aclocal
	touch build/sdk-$(host_platform_arch)/.stamp
	touch $@

build/frida-env-%.rc: build/.clean-sdk-stamp
	FRIDA_HOST=$* ./releng/setup-env.sh


.PHONY: all iconv bfd
.SECONDARY:
