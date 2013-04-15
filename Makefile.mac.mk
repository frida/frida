all: \
	frida-python \
	frida-npapi

include common.mk

distclean:
	rm -rf build/

clean:
	rm -f build/*.rc
	rm -f build/*.site
	rm -f build/*-stamp
	rm -rf build/frida-mac32
	rm -rf build/frida-mac64
	rm -rf build/frida-mac-universal
	rm -rf build/frida-ios
	rm -rf build/tmp-mac32
	rm -rf build/tmp-mac64
	rm -rf build/tmp-mac64-stripped
	rm -rf build/tmp-mac-universal
	rm -rf build/tmp-ios
	rm -rf build/tmp-ios-stripped
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
	source build/frida-env-$*.rc && make -C build/tmp-$*/udis86 install
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
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	touch $@

build/tmp-ios/frida-gum/Makefile: build/frida-env-ios.rc frida-gum/configure
	mkdir -p $(@D)
	source build/frida-env-ios.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-ios/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-ios/frida-gum/Makefile build/frida-gum-submodule-stamp
	find build/tmp-ios/frida-gum -type f -name "*.o" -exec touch {} \;
	source build/frida-env-ios.rc && make -C build/tmp-ios/frida-gum install
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

build/tmp-%/frida-core/tools/frida-resource-compiler: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	find build/tmp-$*/frida-core/tools -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tools
	touch $@

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	find build/tmp-$*/frida-core/lib -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib
	touch $@

build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-mac32/frida-core/lib/agent/libfrida-agent.la build/tmp-mac64/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-mac32/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-32.dylib
	cp build/tmp-mac64/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-64.dylib
	strip -Sx $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib
	lipo $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib -create -output $@

build/tmp-%/frida-core/src/frida-fruitjector-helper: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	find build/tmp-$*/frida-core/src -type f -name "fruitjector-helper*.o" -exec touch {} \;
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/src libfruitjector-types.la frida-fruitjector-helper
	touch $@

build/tmp-mac64-stripped/frida-core/src/frida-fruitjector-helper: build/tmp-mac64/frida-core/src/frida-fruitjector-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	strip -Sx $@.tmp
	codesign -f -s "$$MAC_CERTID" -i "com.tillitech.FridaFruitjectorHelper" $@.tmp
	mv $@.tmp $@

build/tmp-ios-stripped/frida-core/src/frida-fruitjector-helper: build/tmp-ios/frida-core/src/frida-fruitjector-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	strip -Sx $@.tmp
	codesign -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/fruitjector-helper.xcent $@.tmp
	mv $@.tmp $@

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib build/tmp-mac64-stripped/frida-core/src/frida-fruitjector-helper build/tmp-%/frida-core/tools/frida-resource-compiler
	find build/tmp-$*/frida-core/src -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc \
		&& cd build/tmp-$*/frida-core \
		&& make -C src install \
			AGENT=../../../../build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib \
			FRUITJECTOR_HELPER=../../../../build/tmp-mac64-stripped/frida-core/src/frida-fruitjector-helper \
		&& make install-data-am
	touch $@

build/tmp-ios-stripped/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-ios/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp $(<D)/.libs/$(@F) $@.tmp
	strip -Sx $@.tmp
	mv $@.tmp $@

build/frida-ios/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-stripped/frida-core/lib/agent/.libs/libfrida-agent.dylib build/tmp-ios-stripped/frida-core/src/frida-fruitjector-helper build/tmp-mac64/frida-core/tools/frida-resource-compiler
	find build/tmp-ios/frida-core/src -type f -name "*.o" -exec touch {} \;
	source build/frida-env-ios.rc \
		&& cd build/tmp-ios/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER=../../../../build/tmp-mac64/frida-core/tools/frida-resource-compiler \
			AGENT=../../../../build/tmp-ios-stripped/frida-core/lib/agent/.libs/libfrida-agent.dylib \
			FRUITJECTOR_HELPER=../../../../build/tmp-ios-stripped/frida-core/src/frida-fruitjector-helper \
		&& make install-data-am
	touch $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	find build/tmp-$*/frida-core/tests -type f -name "*.o" -exec touch {} \;
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	touch $@

check-core: check-core-mac32 check-core-mac64
check-core-mac32: build/tmp-mac32/frida-core/tests/frida-tests
	$<
check-core-mac64: build/tmp-mac64/frida-core/tests/frida-tests
	$<


frida-python: \
	build/frida-mac-universal/lib/python2.6/site-packages/frida.py \
	build/frida-mac-universal/lib/python2.6/site-packages/_frida.so \
	build/frida-mac-universal/lib/python2.7/site-packages/frida.py \
	build/frida-mac-universal/lib/python2.7/site-packages/_frida.so

frida-python/configure: build/frida-env-mac64.rc frida-python/configure.ac
	source build/frida-env-mac64.rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-python2.6/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/python2.6 ../../../frida-python/configure

build/tmp-%/frida-python2.6/src/_frida.la: build/tmp-%/frida-python2.6/Makefile build/frida-python-submodule-stamp
	touch frida-python/src/_frida.c
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.6 && make install
	touch $@

build/tmp-%/frida-python2.7/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/python2.7 ../../../frida-python/configure

build/tmp-%/frida-python2.7/src/_frida.la: build/tmp-%/frida-python2.7/Makefile build/frida-python-submodule-stamp
	touch frida-python/src/_frida.c
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.7 && make install
	touch $@

build/frida-mac-universal/lib/python%/site-packages/frida.py: build/tmp-mac64/frida-python%/src/_frida.la
	mkdir -p $(@D)
	cp -a build/frida-mac64/lib/python$*/site-packages/frida.py $@
	touch $@

build/frida-mac-universal/lib/python%/site-packages/_frida.so: build/tmp-mac32/frida-python%/src/_frida.la build/tmp-mac64/frida-python%/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-mac32/frida-python$*/src/.libs/_frida.so $(@D)/_frida-32.so
	cp build/tmp-mac64/frida-python$*/src/.libs/_frida.so $(@D)/_frida-64.so
	strip -Sx $(@D)/_frida-32.so $(@D)/_frida-64.so
	lipo $(@D)/_frida-32.so $(@D)/_frida-64.so -create -output $@
	rm $(@D)/_frida-32.so $(@D)/_frida-64.so


frida-npapi: \
	build/frida-mac-universal/lib/browser/plugins/libnpfrida.dylib

frida-npapi/configure: build/frida-env-mac64.rc frida-npapi/configure.ac
	source build/frida-env-mac64.rc && cd frida-npapi && ./autogen.sh

build/tmp-%/frida-npapi/Makefile: build/frida-env-%.rc frida-npapi/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-npapi/configure

build/tmp-%/frida-npapi/src/libnpfrida.la: build/tmp-%/frida-npapi/Makefile build/frida-npapi-submodule-stamp
	touch frida-npapi/src/npfrida-plugin.cpp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-npapi && make install
	touch $@

build/frida-mac-universal/lib/browser/plugins/libnpfrida.dylib: build/tmp-mac32/frida-npapi/src/libnpfrida.la build/tmp-mac64/frida-npapi/src/libnpfrida.la
	mkdir -p $(@D)
	cp build/tmp-mac32/frida-npapi/src/.libs/libnpfrida.dylib $(@D)/libnpfrida-32.dylib
	cp build/tmp-mac64/frida-npapi/src/.libs/libnpfrida.dylib $(@D)/libnpfrida-64.dylib
	strip -Sx $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib
	lipo $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib -create -output $@
	rm $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib


.PHONY: \
	distclean clean check git-submodules git-submodule-stamps \
	udis86 udis86-update-submodule-stamp \
	frida-gum frida-gum-update-submodule-stamp check-gum check-gum-mac32 check-gum-mac64 \
	frida-core frida-core-update-submodule-stamp check-core check-core-mac32 check-core-mac64 \
	frida-python frida-python-update-submodule-stamp \
	frida-npapi frida-npapi-update-submodule-stamp
.SECONDARY:
