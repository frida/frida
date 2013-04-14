all: frida-core frida-python frida-npapi

include common.mk

distclean:
	rm -rf build/

clean:
	rm -f build/*.rc
	rm -f build/*.site
	rm -f build/*-stamp
	rm -rf build/frida-mac32
	rm -rf build/frida-mac64
	rm -rf build/frida-ios
	rm -rf build/tmp-mac32
	rm -rf build/tmp-mac64
	rm -rf build/tmp-ios
	cd udis86 && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-npapi && git clean -xfd

check: check-gum check-core


udis86: \
	build/frida-mac32/lib/pkgconfig/udis86.pc \
	build/frida-mac64/lib/pkgconfig/udis86.pc

udis86/configure: build/frida-env-mac64.rc udis86/configure.ac
	source build/frida-env-mac64.rc && cd udis86 && ./autogen.sh

build/tmp-%/udis86/Makefile: build/frida-env-%.rc udis86/configure
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../udis86/configure

build/frida-%/lib/pkgconfig/udis86.pc: build/tmp-%/udis86/Makefile build/udis86-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/udis86 && make install
	touch $@


frida-gum: \
	build/frida-mac32/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-mac64/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-ios/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: build/frida-env-mac64.rc frida-gum/configure.ac
	source build/frida-env-mac64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/udis86.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	find build/tmp-$*/frida-gum -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-gum && make install
	touch $@

build/tmp-ios/frida-gum/Makefile: build/frida-env-ios.rc frida-gum/configure
	mkdir -p $(@D)
	source build/frida-env-ios.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-ios/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-ios/frida-gum/Makefile build/frida-gum-submodule-stamp
	find build/tmp-ios/frida-gum -type f -name "*.o" -exec touch {} \;
	source build/frida-env-ios.rc && cd build/tmp-ios/frida-gum && make install
	touch $@

check-gum: check-gum-mac32 check-gum-mac64
check-gum-mac32: build/frida-mac32/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-mac32/frida-gum/tests/gum-tests
check-gum-mac64: build/frida-mac64/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-mac64/frida-gum/tests/gum-tests


frida-core: \
	build/frida-mac32/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-mac64/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-ios/lib/pkgconfig/frida-core-1.0.pc

frida-core/configure: build/frida-env-mac64.rc frida-core/configure.ac
	source build/frida-env-mac64.rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	find build/tmp-$*/frida-core -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-core && make install
	touch $@

build/frida-ios/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios/frida-core/Makefile build/frida-core-submodule-stamp build/frida-mac64/lib/pkgconfig/frida-core-1.0.pc
	find build/tmp-ios/frida-core -type f -name "*.o" -exec touch {} \;
	source build/frida-env-ios.rc && cd build/tmp-ios/frida-core && make install RESOURCE_COMPILER=../../../frida-mac64/bin/frida-resource-compiler
	touch $@

check-core: check-core-mac32 check-core-mac64
check-core-mac32: build/frida-mac32/lib/pkgconfig/frida-core-1.0.pc
	build/tmp-mac32/frida-core/tests/frida-tests
check-core-mac64: build/frida-mac64/lib/pkgconfig/frida-core-1.0.pc
	build/tmp-mac64/frida-core/tests/frida-tests


frida-python: \
	build/tmp-mac32/frida-python/src/_frida.la \
	build/tmp-mac64/frida-python/src/_frida.la

frida-python/configure: build/frida-env-mac64.rc frida-python/configure.ac
	source build/frida-env-mac64.rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-python/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-python/configure

build/tmp-%/frida-python/src/_frida.la: build/tmp-%/frida-python/Makefile build/frida-python-submodule-stamp
	touch frida-python/src/_frida.c
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python && make install
	touch $@


frida-npapi: \
	build/tmp-mac32/frida-npapi/src/libnpfrida.la \
	build/tmp-mac64/frida-npapi/src/libnpfrida.la

frida-npapi/configure: build/frida-env-mac64.rc frida-npapi/configure.ac
	source build/frida-env-mac64.rc && cd frida-npapi && ./autogen.sh

build/tmp-%/frida-npapi/Makefile: build/frida-env-%.rc frida-npapi/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-npapi/configure

build/tmp-%/frida-npapi/src/libnpfrida.la: build/tmp-%/frida-npapi/Makefile build/frida-npapi-submodule-stamp
	touch frida-npapi/src/plugin.cpp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-npapi && make install
	touch $@


.PHONY: \
	distclean clean check git-submodules git-submodule-stamps \
	udis86 udis86-update-submodule-stamp \
	frida-gum frida-gum-update-submodule-stamp check-gum check-gum-mac32 check-gum-mac64 \
	frida-core frida-core-update-submodule-stamp check-core check-core-mac32 check-core-mac64 \
	frida-python frida-python-update-submodule-stamp \
	frida-npapi frida-npapi-update-submodule-stamp
.SECONDARY:
