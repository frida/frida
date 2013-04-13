modules = udis86 frida-gum


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
	build/frida-mac32/lib/pkgconfig/udis86.pc \
	build/frida-mac64/lib/pkgconfig/udis86.pc

udis86/configure: build/frida-env-mac64.rc udis86/configure.ac
	source build/frida-env-mac64.rc && cd udis86 && ./autogen.sh

build/tmp-%/udis86/Makefile: build/frida-env-%.rc udis86/configure
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../udis86/configure

build/frida-%/lib/pkgconfig/udis86.pc: build/tmp-%/udis86/Makefile build/udis86-repo-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/udis86 && make install
	touch $@


frida-gum: \
	udis86 \
	build/frida-mac32/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-mac64/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: build/frida-env-mac64.rc frida-gum/configure.ac
	source build/frida-env-mac64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/udis86-repo-stamp build/frida-gum-repo-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-gum && make install
	touch $@


build/frida-env-%.rc:
	FRIDA_TARGET=$* ./setup-env.sh


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
module-stamps: $(foreach m,$(modules),$m-update-repo-stamp)
-include module-stamps


.PHONY: \
	distclean clean module-stamps \
	udis86 udis86-update-repo-stamp \
	frida-gum frida-gum-update-repo-stamp
.SECONDARY:
