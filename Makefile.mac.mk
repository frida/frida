python3 = python3.4

all: \
	frida-server \
	frida-python \
	frida-npapi

include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/*.rc
	rm -f build/*.site
	rm -f build/*-stamp
	rm -rf build/frida-mac-i386
	rm -rf build/frida-mac-x86_64
	rm -rf build/frida-mac-universal
	rm -rf build/frida-ios-universal
	rm -rf build/frida-ios-arm
	rm -rf build/frida-ios-arm64
	rm -rf build/frida-android-arm
	rm -rf build/frida-android-arm-stripped
	rm -rf build/tmp-mac-i386
	rm -rf build/tmp-mac-x86_64
	rm -rf build/tmp-mac-x86_64-stripped
	rm -rf build/tmp-mac-universal
	rm -rf build/tmp-ios-arm
	rm -rf build/tmp-ios-arm-stripped
	rm -rf build/tmp-ios-arm64
	rm -rf build/tmp-ios-arm64-stripped
	rm -rf build/tmp-ios-universal
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-android-arm-stripped

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-npapi && git clean -xfd

check: check-gum check-core


capstone: \
	build/frida-mac-i386/lib/pkgconfig/capstone.pc \
	build/frida-mac-x86_64/lib/pkgconfig/capstone.pc \
	build/frida-ios-arm/lib/pkgconfig/capstone.pc \
	build/frida-ios-arm64/lib/pkgconfig/capstone.pc

build/frida-%/lib/pkgconfig/capstone.pc: build/frida-env-%.rc build/capstone-submodule-stamp
	source build/frida-env-$*.rc \
		&& export PACKAGE_TARNAME=capstone \
		&& source $$CONFIG_SITE \
		&& make -C capstone \
			PREFIX=$$frida_prefix \
			BUILDDIR=../build/tmp-$*/capstone \
			CAPSTONE_ARCHS="arm aarch64 x86" \
			CAPSTONE_SHARED=$$enable_shared \
			CAPSTONE_STATIC=$$enable_static \
			install


frida-gum: \
	build/frida-mac-i386/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-mac-x86_64/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-ios-arm/lib/pkgconfig/frida-gum-1.0.pc \
	build/frida-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: build/frida-env-mac-x86_64.rc frida-gum/configure.ac
	source build/frida-env-mac-x86_64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum: check-gum-mac-i386 check-gum-mac-x86_64
check-gum-mac-i386: build/frida-mac-i386/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-mac-i386/frida-gum/tests/gum-tests
check-gum-mac-x86_64: build/frida-mac-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-mac-x86_64/frida-gum/tests/gum-tests


frida-core: \
	build/frida-mac-i386/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-mac-x86_64/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc \
	build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc

frida-core/configure: build/frida-env-mac-x86_64.rc frida-core/configure.ac
	source build/frida-env-mac-x86_64.rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/tmp-%/frida-core/tools/resource-compiler: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/tools/resource-compiler.c,build/tmp-$*/frida-core/tools/frida_resource_compiler-resource-compiler.o)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tools
	@touch -c $@

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/agent/agent.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_la-agent.lo)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib
	@touch -c $@

build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-mac-i386/frida-core/lib/agent/libfrida-agent.la build/tmp-mac-x86_64/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-32.dylib
	cp build/tmp-mac-x86_64/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-64.dylib
	strip -Sx $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib
	lipo $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib -create -output $@

build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-ios-arm/frida-core/lib/agent/libfrida-agent.la build/tmp-ios-arm64/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-ios-arm/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-32.dylib
	cp build/tmp-ios-arm64/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-64.dylib
	strip -Sx $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib
	lipo $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib -create -output $@

build/tmp-%/frida-core/src/frida-helper: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/src/darwin/frida-helper-glue.c,build/tmp-$*/frida-core/src/frida-helper-glue.lo)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/src libfrida-helper-types.la frida-helper.stamp
	@touch -c $@

build/tmp-mac-x86_64-stripped/frida-core/src/frida-helper: build/tmp-mac-x86_64/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	strip -Sx $@.tmp
	codesign -f -s "$$MAC_CERTID" -i "re.frida.Helper" $@.tmp
	mv $@.tmp $@

build/tmp-ios-arm-stripped/frida-core/src/frida-helper: build/tmp-ios-arm/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	strip -Sx $@.tmp
	codesign -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@

build/tmp-ios-arm64-stripped/frida-core/src/frida-helper: build/tmp-ios-arm64/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	strip -Sx $@.tmp
	codesign -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@

build/tmp-ios-universal/frida-core/src/frida-helper: build/tmp-ios-arm-stripped/frida-core/src/frida-helper build/tmp-ios-arm64-stripped/frida-core/src/frida-helper
	mkdir -p $(@D)
	lipo $^ -create -output $@.tmp
	codesign -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@

build/tmp-%-stripped/frida-core/lib/agent/.libs/libfrida-agent.so: build/tmp-%/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/agent/.libs/libfrida-agent.so $@
	source build/frida-env-$*.rc && $$STRIP --strip-all $@

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib build/tmp-mac-x86_64-stripped/frida-core/src/frida-helper build/tmp-%/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-$*/frida-core/src/libfrida_core_la-frida.lo)
	source build/frida-env-$*.rc \
		&& cd build/tmp-$*/frida-core \
		&& make -C src install \
			AGENT=../../../../build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib \
			HELPER=../../../../build/tmp-mac-x86_64-stripped/frida-core/src/frida-helper \
		&& make install-data-am
	@touch -c $@

build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib build/tmp-ios-universal/frida-core/src/frida-helper build/tmp-mac-x86_64/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-ios-arm/frida-core/src/libfrida_core_la-frida.lo)
	source build/frida-env-ios-arm.rc \
		&& cd build/tmp-ios-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER=../../../../build/tmp-mac-x86_64/frida-core/tools/resource-compiler \
			AGENT=../../../../build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib \
			HELPER=../../../../build/tmp-ios-universal/frida-core/src/frida-helper \
		&& make install-data-am
	@touch -c $@

build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib build/tmp-ios-universal/frida-core/src/frida-helper build/tmp-mac-x86_64/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-ios-arm64/frida-core/src/libfrida_core_la-frida.lo)
	source build/frida-env-ios-arm64.rc \
		&& cd build/tmp-ios-arm64/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER=../../../../build/tmp-mac-x86_64/frida-core/tools/resource-compiler \
			AGENT=../../../../build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib \
			HELPER=../../../../build/tmp-ios-arm64-stripped/frida-core/src/frida-helper \
		&& make install-data-am
	@touch -c $@

build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm-stripped/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp-mac-x86_64/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-arm/frida-core/src/libfrida_core_la-frida.lo)
	source build/frida-env-android-arm.rc \
		&& cd build/tmp-android-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="../../../../build/tmp-mac-x86_64/frida-core/tools/resource-compiler --toolchain=gnu" \
			AGENT=../../../../build/tmp-android-arm-stripped/frida-core/lib/agent/.libs/libfrida-agent.so \
		&& make install-data-am
	@touch -c $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core: check-core-mac-i386 check-core-mac-x86_64
check-core-mac-i386: build/tmp-mac-i386/frida-core/tests/frida-tests
	$<
check-core-mac-x86_64: build/tmp-mac-x86_64/frida-core/tests/frida-tests
	$<


frida-server: \
	build/frida-ios-universal/bin/frida-server

build/frida-ios-universal/bin/frida-server: build/frida-ios-arm/bin/frida-server build/frida-ios-arm64/bin/frida-server
	mkdir -p $(@D)
	cp build/frida-ios-arm/bin/frida-server $(@D)/frida-server-32
	cp build/frida-ios-arm64/bin/frida-server $(@D)/frida-server-64
	strip -Sx $(@D)/frida-server-32 $(@D)/frida-server-64
	lipo $(@D)/frida-server-32 $(@D)/frida-server-64 -create -output $@
	$(RM) $(@D)/frida-server-32 $(@D)/frida-server-64

build/frida-android-arm-stripped/bin/frida-server: build/frida-android-arm/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	source build/frida-env-android-arm.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/frida-%/bin/frida-server: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/server/server.c,build/tmp-$*/frida-core/server/frida_server-server.o)
	source build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/server install
	@touch -c $@


frida-python: frida-python2 frida-python3

frida-python2: \
	build/frida-mac-universal/lib/python2.6/site-packages/frida \
	build/frida-mac-universal/lib/python2.6/site-packages/_frida.so \
	build/frida-mac-universal/lib/python2.7/site-packages/frida \
	build/frida-mac-universal/lib/python2.7/site-packages/_frida.so

frida-python3: \
	build/frida-mac-universal/lib/$(python3)/site-packages/frida \
	build/frida-mac-universal/lib/$(python3)/site-packages/_frida.so

frida-python/configure: build/frida-env-mac-x86_64.rc frida-python/configure.ac
	source build/frida-env-mac-x86_64.rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-python2.6/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/python2.6 ../../../frida-python/configure

build/tmp-%/frida-python2.6/src/_frida.la: build/tmp-%/frida-python2.6/Makefile build/frida-python-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.6 && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-python2.6/src/_frida.lo)
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.6 && make install
	@touch -c $@

build/tmp-%/frida-python2.7/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/python2.7 ../../../frida-python/configure

build/tmp-%/frida-python2.7/src/_frida.la: build/tmp-%/frida-python2.7/Makefile build/frida-python-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.7 && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-python2.7/src/_frida.lo)
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.7 && make install
	@touch -c $@

build/tmp-%/frida-$(python3)/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/local/bin/$(python3) ../../../frida-python/configure

build/tmp-%/frida-$(python3)/src/_frida.la: build/tmp-%/frida-$(python3)/Makefile build/frida-python-submodule-stamp
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-$(python3) && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-$(python3)/src/_frida.lo)
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-$(python3) && make install
	@touch -c $@

build/frida-mac-universal/lib/python%/site-packages/frida: build/tmp-mac-x86_64/frida-python%/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-mac-x86_64/lib/python$*/site-packages/frida $@
	@touch $@

build/frida-mac-universal/lib/python%/site-packages/_frida.so: build/tmp-mac-i386/frida-python%/src/_frida.la build/tmp-mac-x86_64/frida-python%/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-python$*/src/.libs/_frida.so $(@D)/_frida-32.so
	cp build/tmp-mac-x86_64/frida-python$*/src/.libs/_frida.so $(@D)/_frida-64.so
	strip -Sx $(@D)/_frida-32.so $(@D)/_frida-64.so
	lipo $(@D)/_frida-32.so $(@D)/_frida-64.so -create -output $@
	rm $(@D)/_frida-32.so $(@D)/_frida-64.so

check-python: check-python2 check-python3

check-python2: frida-python2
	export PYTHONPATH="$(shell pwd)/build/frida-mac-universal/lib/python2.6/site-packages" \
		&& cd frida-python \
		&& unit2 discover
	export PYTHONPATH="$(shell pwd)/build/frida-mac-universal/lib/python2.7/site-packages" \
		&& cd frida-python \
		&& python2.7 -m unittest discover

check-python3: frida-python3
	export PYTHONPATH="$(shell pwd)/build/frida-mac-universal/lib/$(python3)/site-packages" \
		&& cd frida-python \
		&& $(python3) -m unittest discover


frida-npapi: \
	build/frida-mac-universal/lib/browser/plugins/libnpfrida.dylib

frida-npapi/configure: build/frida-env-mac-x86_64.rc frida-npapi/configure.ac build/frida-mac-x86_64/lib/pkgconfig/frida-core-1.0.pc
	source build/frida-env-mac-x86_64.rc \
		&& pushd frida-npapi >/dev/null \
		&& ./autogen.sh \
		&& popd >/dev/null \
		&& mkdir -p build/tmp-mac-x86_64/frida-npapi \
		&& pushd build/tmp-mac-x86_64/frida-npapi >/dev/null \
		&& ../../../frida-npapi/configure \
		&& rm -f ../../../frida-npapi/src/libnpfrida_codegen_la_vala.stamp \
		&& make -C src ../../../../frida-npapi/src/libnpfrida_codegen_la_vala.stamp \
		&& popd >/dev/null

build/tmp-%/frida-npapi/Makefile: build/frida-env-%.rc frida-npapi/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	source build/frida-env-$*.rc && cd $(@D) && ../../../frida-npapi/configure

build/tmp-%/frida-npapi/src/libnpfrida.la: build/tmp-%/frida-npapi/Makefile build/frida-npapi-submodule-stamp
	@$(call ensure_relink,frida-npapi/src/npfrida-plugin.cpp,build/tmp-$*/frida-npapi/src/npfrida-plugin.lo)
	source build/frida-env-$*.rc && cd build/tmp-$*/frida-npapi && make install
	@touch -c $@

build/frida-mac-universal/lib/browser/plugins/libnpfrida.dylib: build/tmp-mac-i386/frida-npapi/src/libnpfrida.la build/tmp-mac-x86_64/frida-npapi/src/libnpfrida.la
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-npapi/src/.libs/libnpfrida.dylib $(@D)/libnpfrida-32.dylib
	cp build/tmp-mac-x86_64/frida-npapi/src/.libs/libnpfrida.dylib $(@D)/libnpfrida-64.dylib
	strip -Sx $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib
	lipo $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib -create -output $@
	rm $(@D)/libnpfrida-32.dylib $(@D)/libnpfrida-64.dylib


.PHONY: \
	distclean clean clean-submodules check git-submodules git-submodule-stamps \
	capstone capstone-update-submodule-stamp \
	frida-gum frida-gum-update-submodule-stamp check-gum check-gum-mac-i386 check-gum-mac-x86_64 \
	frida-core frida-core-update-submodule-stamp check-core check-core-mac-i386 check-core-mac-x86_64 \
	frida-server \
	frida-python frida-python2 frida-python3 frida-python-update-submodule-stamp check-python check-python2 check-python3 \
	frida-npapi frida-npapi-update-submodule-stamp
.SECONDARY:
