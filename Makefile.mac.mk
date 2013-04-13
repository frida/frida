mac32 = build/tmp-mac32
mac64 = build/tmp-mac64
ios = build/tmp-ios
udis86 = ../../../udis86
frida_gum = ../../../frida-gum


all: udis86 frida-gum

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


udis86: \
	udis86-update-repo-stamp \
	build/frida-mac32/lib/pkgconfig/udis86.pc \
	build/frida-mac64/lib/pkgconfig/udis86.pc

udis86/configure: env-mac64 udis86/configure.ac
	source build/frida-env-mac64.rc && cd udis86 && ./autogen.sh

build/tmp-%/udis86/Makefile: udis86/configure
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && $(udis86)/configure

build/frida-%/lib/pkgconfig/udis86.pc: env-% udis86/configure build/tmp-%/udis86/Makefile build/udis86-repo-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/udis86 && make install
	touch $@


udis86-update-repo-stamp:
	@cd udis86 && git log -1 --format=%H > ../build/udis86-repo-stamp.tmp && git status >> ../build/udis86-repo-stamp.tmp
	@if [ -f build/udis86-repo-stamp ]; then \
		if cmp -s build/udis86-repo-stamp build/udis86-repo-stamp.tmp; then \
			rm build/udis86-repo-stamp.tmp; \
		else \
			mv build/udis86-repo-stamp.tmp build/udis86-repo-stamp; \
		fi \
	else \
		mv build/udis86-repo-stamp.tmp build/udis86-repo-stamp; \
	fi


env-mac32: build/frida-env-mac32-stamp
build/frida-env-mac32-stamp:
	FRIDA_TARGET=mac32 ./setup-env.sh
	mkdir -p $(mac32)
	touch build/frida-env-mac32-stamp

env-mac64: build/frida-env-mac64-stamp
build/frida-env-mac64-stamp:
	FRIDA_TARGET=mac64 ./setup-env.sh
	mkdir -p $(mac64)
	touch build/frida-env-mac64-stamp

env-ios: build/frida-env-ios-stamp
build/frida-env-ios-stamp:
	FRIDA_TARGET=ios ./setup-env.sh
	mkdir -p $(ios)
	touch build/frida-env-ios-stamp


.PHONY: \
	distclean clean \
	udis86 udis86-update-repo-stamp
.INTERMEDIATE: \
	env-mac32 env-mac64 env-ios
.SECONDARY:
