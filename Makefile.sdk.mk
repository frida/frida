MAKE_J ?= -j 8
REPO_BASE_URL = "git://github.com/frida"
REPO_SUFFIX = ".git"

plain_modules = libffi glib libgee json-glib

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

prefix := build/frida-$(host_platform_arch)


all: iconv bfd $(prefix)/lib/pkgconfig/libffi.pc $(prefix)/lib/pkgconfig/glib.pc


ifeq ($(host_platform), linux)
iconv:
bfd: $(prefix)/lib/libbfd.a
else
ifeq ($(host_platform), android)
iconv: $(prefix)/lib/libiconv.a
bfd: $(prefix)/lib/libbfd.a
else
iconv:
bfd:
endif
endif


binutils:
	$(RM) -rf binutils.tmp
	mkdir binutils.tmp
	cd binutils.tmp \
		&& $(download) http://gnuftp.uib.no/binutils/$@-2.24.tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/binutils-android.patch
	mv binutils.tmp $@

build/tmp-%/binutils/libiberty/Makefile: binutils build/frida-env-%.rc
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../../binutils/libiberty/configure

build/tmp-%/binutils/bfd/Makefile: binutils build/frida-env-%.rc
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../../binutils/bfd/configure

build/frida-%/lib/libbfd.a: \
		build/frida-%/include/bfd.h \
		build/tmp-%/binutils/libiberty/libiberty.a \
		build/tmp-%/binutils/bfd/libbfd.a \
		build/frida-env-%.rc
	mkdir -p $(@D)
	rm -rf build/tmp-$*/binutils/tmp
	mkdir build/tmp-$*/binutils/tmp
	. build/frida-env-$*.rc \
		&& cd build/tmp-$*/binutils/tmp \
		&& $$AR x ../libiberty/libiberty.a \
		&& $$AR x ../bfd/libbfd.a \
		&& $$AR r libbfd-full.a *.o \
		&& $$RANLIB libbfd-full.a \
		&& install -m 644 libbfd-full.a ../../../../$@

build/frida-%/include/bfd.h: build/tmp-%/binutils/bfd/Makefile build/frida-env-%.rc
	. build/frida-env-$*.rc && make -C build/tmp-$*/binutils/bfd $(MAKE_J) install-bfdincludeHEADERS

build/tmp-%/binutils/libiberty/libiberty.a: build/tmp-%/binutils/libiberty/Makefile build/frida-env-%.rc
	. build/frida-env-$*.rc && make -C $(@D) $(MAKE_J)

build/tmp-%/binutils/bfd/libbfd.a: build/tmp-%/binutils/bfd/Makefile build/frida-env-%.rc
	. build/frida-env-$*.rc && make -C $(@D) $(MAKE_J)


define make-plain-module-rules
$1:
	git clone $(REPO_BASE_URL)/$$@$(REPO_SUFFIX)

$1/configure: build/frida-env-$(build_platform_arch).rc $1
	. $$< && cd $$(@D) && NOCONFIGURE=1 ./autogen.sh && touch configure

build/tmp-%/$1/Makefile: $1/configure build/frida-env-%.rc
	$(RM) -rf $$(@D)
	mkdir -p $$(@D)
	. build/frida-env-$$*.rc && cd $$(@D) && ../../../$1/configure

build/frida-%/lib/pkgconfig/$1.pc: build/tmp-%/$1/Makefile
	. build/frida-env-$$*.rc && make -C build/tmp-$$*/$1 $(MAKE_J) install GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums
endef
$(foreach m,$(plain_modules),$(eval $(call make-plain-module-rules,$m)))
plain-modules: $(foreach m,$(modules),$m)
-include plain-modules


build/.clean-sdk-stamp:
	rm -rf build
	mkdir -p build/sdk-$(host_platform_arch)/share/aclocal
	touch build/sdk-$(host_platform_arch)/.stamp
	touch $@

build/frida-env-%.rc: build/.clean-sdk-stamp
	FRIDA_HOST=$* ./releng/setup-env.sh


.PHONY: all iconv bfd
.SECONDARY:
