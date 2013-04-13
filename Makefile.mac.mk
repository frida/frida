modules = udis86 frida-gum
mac32 = build/tmp-mac32
mac64 = build/tmp-mac64
ios = build/tmp-ios


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
	source build/frida-env-$*.rc && cd $(@D) && ../../../udis86/configure

build/frida-%/lib/pkgconfig/udis86.pc: env-% udis86/configure build/tmp-%/udis86/Makefile build/udis86-repo-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/udis86 && make install
	touch $@


frida-gum: \
	frida-gum-update-repo-stamp \
	build/frida-mac32/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-mac64/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: env-mac64 frida-gum/configure.ac
	source build/frida-env-mac64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: frida-gum/configure build/frida-%/lib/pkgconfig/udis86.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: env-% frida-gum/configure build/tmp-%/frida-gum/Makefile build/frida-gum-repo-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-gum && make install
	touch $@


define make-update-repo-stamp
$1-update-repo-stamp:
	@cd $1 && git log -1 --format=%H > ../build/$1-repo-stamp.tmp && git status >> ../build/$1-repo-stamp.tmp
	@if [ -f build/$1-repo-stamp ]; then \
		if cmp -s build/$1-repo-stamp build/$1-repo-stamp.tmp; then \
			rm build/$1-repo-stamp.tmp; \
		else \
			mv build/$1-repo-stamp.tmp build/$1-repo-stamp; \
		fi \
	else \
		mv build/$1-repo-stamp.tmp build/$1-repo-stamp; \
	fi
endef
$(foreach m,$(modules),$(eval $(call make-update-repo-stamp,$m)))


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
	udis86 udis86-update-repo-stamp \
	frida-gum frida-gum-update-repo-stamp
.INTERMEDIATE: \
	env-mac32 env-mac64 env-ios
.SECONDARY:
