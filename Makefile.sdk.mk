MAKE_J ?= -j 8

build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,mac,')

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

all: iconv bfd
	@echo "host_platform: '$(host_platform)'"
	@echo "  host_distro: '$(host_distro)'"
	@echo "    host_arch: '$(host_arch)'"

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

build/.clean-sdk-stamp:
	rm -rf build
	mkdir -p build/sdk-$(host_platform_arch)/share/aclocal
	touch build/sdk-$(host_platform_arch)/.stamp
	touch $@

build/frida-env-%.rc: build/.clean-sdk-stamp
	FRIDA_HOST=$* ./releng/setup-env.sh

build/frida-%/lib/libbfd.a: build/tmp-%/binutils/libiberty/libiberty.a build/tmp-%/binutils/bfd/libbfd.a build/frida-env-%.rc
	@echo "TODO"

build/tmp-%/binutils/libiberty/Makefile: build/frida-env-%.rc binutils
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../../binutils/libiberty/configure

build/tmp-%/binutils/bfd/Makefile: build/frida-env-%.rc binutils
	$(RM) -rf $(@D)
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../../binutils/bfd/configure

build/tmp-%/binutils/libiberty/libiberty.a: build/tmp-%/binutils/libiberty/Makefile
	. build/frida-env-$*.rc && cd $(@D) && make $(MAKE_J)

build/tmp-%/binutils/bfd/libbfd.a: build/tmp-%/binutils/bfd/Makefile
	. build/frida-env-$*.rc && cd $(@D) && make $(MAKE_J)

binutils:
	$(RM) -rf binutils.tmp
	mkdir binutils.tmp
	cd binutils.tmp \
		&& $(download) http://gnuftp.uib.no/binutils/$@-2.24.tar.bz2 | tar -xj --strip-components 1 \
		&& patch -p1 < ../releng/patches/binutils-android.patch
	mv binutils.tmp $@

.PHONY: all iconv bfd
.SECONDARY:
