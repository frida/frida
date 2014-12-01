python3 = python3.4

all: \
	frida-python \
	frida-npapi

include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/*.rc
	rm -f build/*.site
	rm -f build/*-stamp
	rm -rf build/frida-linux-x86_64
	rm -rf build/frida-linux-x86_64-stripped
	rm -rf build/tmp-linux-x86_64
	rm -rf build/tmp-linux-x86_64-stripped

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-npapi && git clean -xfd

check: check-gum check-core


capstone: \
	build/frida-linux-x86_64/lib/pkgconfig/capstone.pc

build/frida-%/lib/pkgconfig/capstone.pc: build/frida-env-%.rc build/capstone-submodule-stamp
	. build/frida-env-$*.rc \
		&& export PACKAGE_TARNAME=capstone \
		&& . $$CONFIG_SITE \
		&& make -C capstone \
			PREFIX=$$frida_prefix \
			BUILDDIR=../build/tmp-$*/capstone \
			CAPSTONE_ARCHS="arm aarch64 x86" \
			CAPSTONE_SHARED=$$enable_shared \
			CAPSTONE_STATIC=$$enable_static \
			install


frida-gum: \
	build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc

frida-gum/configure: build/frida-env-linux-x86_64.rc frida-gum/configure.ac
	. build/frida-env-linux-x86_64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum: check-gum-linux-x86_64
check-gum-linux-x86_64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-linux-x86_64/frida-gum/tests/gum-tests


frida-core: \
	build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc

frida-core/configure: build/frida-env-linux-x86_64.rc frida-core/configure.ac
	. build/frida-env-linux-x86_64.rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/tmp-%/frida-core/tools/resource-compiler: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/tools/resource-compiler.c,build/tmp-$*/frida-core/tools/frida_resource_compiler-resource-compiler.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tools
	@touch -c $@

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/agent/agent.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_la-agent.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib
	@touch -c $@

build/tmp-%-stripped/frida-core/lib/agent/.libs/libfrida-agent.so: build/tmp-%/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/agent/.libs/libfrida-agent.so $@
	strip --strip-all $@

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-%-stripped/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp-%/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-$*.rc \
		&& cd build/tmp-$*/frida-core \
		&& make -C src install \
			AGENT=../../../../build/tmp-$*-stripped/frida-core/lib/agent/.libs/libfrida-agent.so \
		&& make install-data-am
	@touch -c $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core: check-core-linux-x86_64
check-core-linux-x86_64: build/tmp-linux-x86_64/frida-core/tests/frida-tests
	$<


frida-server: \
	build/frida-android-arm-stripped/bin/frida-server

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
	build/frida-linux-x86_64-stripped/lib/python2.7/site-packages/frida \
	build/frida-linux-x86_64-stripped/lib/python2.7/site-packages/_frida.so

frida-python3: \
	build/frida-linux-x86_64-stripped/lib/${python3}/site-packages/frida \
	build/frida-linux-x86_64-stripped/lib/${python3}/site-packages/_frida.so

frida-python/configure: build/frida-env-linux-x86_64.rc frida-python/configure.ac
	. build/frida-env-linux-x86_64.rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-python2.7/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/python2.7 ../../../frida-python/configure

build/tmp-%/frida-python2.7/src/_frida.la: build/tmp-%/frida-python2.7/Makefile build/frida-python-submodule-stamp
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.7 && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-python2.7/src/_frida.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-python2.7 && make install
	@touch -c $@

build/frida-%-stripped/lib/python2.7/site-packages/frida: build/tmp-linux-x86_64/frida-python2.7/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-$*/lib/python2.7/site-packages/frida $@
	@touch $@

build/frida-%-stripped/lib/python2.7/site-packages/_frida.so: build/tmp-linux-x86_64/frida-python2.7/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-python2.7/src/.libs/_frida.so $@
	strip --strip-all $@

build/tmp-%/frida-${python3}/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && PYTHON=/usr/bin/${python3} ../../../frida-python/configure

build/tmp-%/frida-${python3}/src/_frida.la: build/tmp-%/frida-${python3}/Makefile build/frida-python-submodule-stamp
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-${python3} && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-${python3}/src/_frida.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-${python3} && make install
	@touch -c $@

build/frida-%-stripped/lib/${python3}/site-packages/frida: build/tmp-linux-x86_64/frida-${python3}/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-$*/lib/${python3}/site-packages/frida $@
	@touch $@

build/frida-%-stripped/lib/${python3}/site-packages/_frida.so: build/tmp-linux-x86_64/frida-${python3}/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-${python3}/src/.libs/_frida.so $@
	strip --strip-all $@

check-python: check-python2 check-python3

check-python2: frida-python2
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64-stripped/lib/python2.7/site-packages" \
		&& cd frida-python \
		&& python2.7 -m unittest tests.test_core tests.test_tracer

check-python3: frida-python3
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64-stripped/lib/${python3}/site-packages" \
		&& cd frida-python \
		&& ${python3} -m unittest tests.test_core tests.test_tracer


frida-npapi: \
	build/frida-linux-x86_64-stripped/lib/browser/plugins/libnpfrida.so

frida-npapi/configure: build/frida-env-linux-x86_64.rc frida-npapi/configure.ac build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc
	. build/frida-env-linux-x86_64.rc \
		&& cd frida-npapi \
		&& ./autogen.sh \
		&& cd .. \
		&& mkdir -p build/tmp-linux-x86_64/frida-npapi \
		&& cd build/tmp-linux-x86_64/frida-npapi \
		&& ../../../frida-npapi/configure \
		&& rm -f ../../../frida-npapi/src/libnpfrida_codegen_la_vala.stamp \
		&& make -C src ../../../../frida-npapi/src/libnpfrida_codegen_la_vala.stamp

build/tmp-%/frida-npapi/Makefile: build/frida-env-%.rc frida-npapi/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-npapi/configure

build/tmp-%/frida-npapi/src/libnpfrida.la: build/tmp-%/frida-npapi/Makefile build/frida-npapi-submodule-stamp
	@$(call ensure_relink,frida-npapi/src/npfrida-plugin.cpp,build/tmp-$*/frida-npapi/src/npfrida-plugin.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-npapi && make install
	@touch -c $@

build/frida-%-stripped/lib/browser/plugins/libnpfrida.so: build/tmp-%/frida-npapi/src/libnpfrida.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-npapi/src/.libs/libnpfrida.so $@
	strip --strip-all $@


.PHONY: \
	distclean clean clean-submodules check git-submodules git-submodule-stamps \
	capstone capstone-update-submodule-stamp \
	frida-gum frida-gum-update-submodule-stamp check-gum check-gum-linux-x86_64 \
	frida-core frida-core-update-submodule-stamp check-core check-core-linux-x86_64 \
	frida-server \
	frida-python frida-python2 frida-python3 frida-python-update-submodule-stamp check-python check-python2 check-python3 \
	frida-npapi frida-npapi-update-submodule-stamp
.SECONDARY:
