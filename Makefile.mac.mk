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
	rm -f build/*.stamp
	rm -rf build/frida-mac32
	rm -rf build/frida-mac64
	rm -rf build/frida-ios
	rm -rf build/tmp-mac32
	rm -rf build/tmp-mac64
	rm -rf build/tmp-ios


udis86: udis86-mac32 udis86-mac64

udis86-mac32: env-mac32 udis86-autogen udis86-mac32-makefile
	source build/frida-env-mac32.rc && cd $(mac32)/udis86 && make install

udis86-mac64: env-mac64 udis86-autogen udis86-mac64-makefile
	source build/frida-env-mac64.rc && cd $(mac64)/udis86 && make install

udis86-autogen: build/udis86-autogen.stamp
build/udis86-autogen.stamp: udis86/configure.ac env-mac64
	source build/frida-env-mac64.rc && cd udis86 && ./autogen.sh
	@touch $@

udis86-mac32-makefile: $(mac32)/udis86-makefile.stamp
$(mac32)/udis86-makefile.stamp: udis86/configure
	mkdir -p $(mac32)/udis86
	source build/frida-env-mac32.rc && cd $(mac32)/udis86 && $(udis86)/configure
	@touch $@

udis86-mac64-makefile: $(mac64)/udis86-makefile.stamp
$(mac64)/udis86-makefile.stamp: udis86/configure
	mkdir -p $(mac64)/udis86
	source build/frida-env-mac64.rc && cd $(mac64)/udis86 && $(udis86)/configure
	@touch $@


frida-gum: frida-gum-mac32 frida-gum-mac64

frida-gum-mac32: env-mac32 frida-gum-autogen frida-gum-mac32-makefile
	source build/frida-env-mac32.rc && cd $(mac32)/frida-gum && make install

frida-gum-mac64: env-mac64 frida-gum-autogen frida-gum-mac64-makefile
	source build/frida-env-mac64.rc && cd $(mac64)/frida-gum && make install

frida-gum-autogen: build/frida-gum-autogen.stamp env-mac64
build/frida-gum-autogen.stamp: frida-gum/configure.ac
	source build/frida-env-mac64.rc && cd frida-gum && ./autogen.sh
	@touch $@

frida-gum-mac32-makefile: $(mac32)/frida-gum-makefile.stamp
$(mac32)/frida-gum-makefile.stamp: udis86-mac32 frida-gum/configure
	mkdir -p $(mac32)/frida-gum
	source build/frida-env-mac32.rc && cd $(mac32)/frida-gum && $(frida_gum)/configure
	@touch $@

frida-gum-mac64-makefile: $(mac64)/frida-gum-makefile.stamp
$(mac64)/frida-gum-makefile.stamp: udis86-mac64 frida-gum/configure
	mkdir -p $(mac64)/frida-gum
	source build/frida-env-mac64.rc && cd $(mac64)/frida-gum && $(frida_gum)/configure
	@touch $@


env-mac32: build/frida-env-mac32.stamp
build/frida-env-mac32.stamp:
	FRIDA_TARGET=mac32 ./setup-env.sh
	mkdir -p $(mac32)
	touch build/frida-env-mac32.stamp

env-mac64: build/frida-env-mac64.stamp
build/frida-env-mac64.stamp:
	FRIDA_TARGET=mac64 ./setup-env.sh
	mkdir -p $(mac64)
	touch build/frida-env-mac64.stamp

env-ios: build/frida-env-ios.stamp
build/frida-env-ios.stamp:
	FRIDA_TARGET=ios ./setup-env.sh
	mkdir -p $(ios)
	touch build/frida-env-ios.stamp
