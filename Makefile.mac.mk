all: frida-core

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
	build/frida-mac64/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: build/frida-env-mac64.rc frida-gum/configure.ac
	source build/frida-env-mac64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/udis86.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-gum && make install
	touch $@


frida-core: \
	build/frida-mac32/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-mac64/lib/pkgconfig/frida-core-1.0.pc

frida-core/configure: build/frida-env-mac64.rc frida-core/configure.ac
	source build/frida-env-mac64.rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-core && make install
	touch $@


.PHONY: \
	distclean clean git-submodules git-submodule-stamps \
	udis86 udis86-update-submodule-stamp \
	frida-gum frida-gum-update-submodule-stamp \
	frida-core frida-core-update-submodule-stamp
.SECONDARY:
