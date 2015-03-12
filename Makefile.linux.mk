FRIDA := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

PYTHON ?= $(shell readlink -f $(shell which python) 2>/dev/null)
PYTHON_NAME ?= $(shell basename $(PYTHON))

NODE ?= $(shell readlink -f $(shell which node) 2>/dev/null)
NODE_BIN_DIR := $(shell dirname $(NODE) 2>/dev/null)
NPM ?= $(NODE_BIN_DIR)/npm

all: help

HELP_FUN = \
	%help; \
	while(<>) { push @{$$help{$$2 // 'options'}}, [$$1, $$3] if /^([\w-]+)\s*:.*\#\#(?:@([\w-]+))?\s(.*)$$/ }; \
	print "usage: make -f Makefile.linux.mk [target]\n\n"; \
	print "TARGETS:\n"; \
	for (keys %help) { \
		print "  $$_:\n"; $$sep = " " x (20 - length $$_->[0]); \
		printf("    %-20s    %s\n", $$_->[0], $$_->[1]) for @{$$help{$$_}}; \
		print "\n"; \
	} \
	print "VARIABLES:\n"; \
        print "  bindings:\n"; \
	print "    PYTHON                  Absolute path of Python interpreter including version suffix\n"; \
	print "    NODE                    Absolute path of Node.js binary\n"; \
	print "\n"; \
	print "EXAMPLES:\n"; \
	print "  \$$ make -f Makefile.linux.mk python-64 PYTHON=/opt/python34-64/bin/python-3.4\n"; \
	print "  \$$ make -f Makefile.linux.mk node-32 NODE=/opt/node-32/bin/node\n"; \
	print "\n";

help:
	@LC_ALL=C perl -e '$(HELP_FUN)' $(MAKEFILE_LIST)

include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/*.rc
	rm -f build/*.site
	rm -f build/*-stamp
	rm -rf build/frida-linux-i386
	rm -rf build/frida-linux-i386-stripped
	rm -rf build/frida-linux-x86_64
	rm -rf build/frida-linux-x86_64-stripped
	rm -rf build/frida-android-i386
	rm -rf build/frida-android-i386-stripped
	rm -rf build/frida-android-arm
	rm -rf build/frida-android-arm-stripped
	rm -rf build/tmp-linux-i386
	rm -rf build/tmp-linux-i386-stripped
	rm -rf build/tmp-linux-x86_64
	rm -rf build/tmp-linux-x86_64-stripped
	rm -rf build/tmp-android-i386
	rm -rf build/tmp-android-i386-stripped
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-android-arm-stripped

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd


capstone-32: build/frida-linux-i386/lib/pkgconfig/capstone.pc
capstone-64: build/frida-linux-x86_64/lib/pkgconfig/capstone.pc

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


gum-32: build/frida-linux-i386/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for i386
gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86-64

frida-gum/configure: build/frida-env-linux-x86_64.rc frida-gum/configure.ac
	. build/frida-env-linux-x86_64.rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum-32: check-gum-linux-i386 ##@gum Run tests for i386
check-gum-64: check-gum-linux-x86_64 ##@gum Run tests for x86-64
check-gum-linux-%: build/frida-linux-%/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-linux-$*/frida-gum/tests/gum-tests


core-32: build/frida-linux-i386/lib/pkgconfig/frida-core-1.0.pc ##@core Build for i386
core-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86-64

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
	. build/frida-env-$*.rc && $$STRIP --strip-all $@

build/frida-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-%-stripped/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp-linux-x86_64/frida-core/tools/resource-compiler
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-$*.rc \
		&& cd build/tmp-$*/frida-core \
		&& make -C src install \
		 	RESOURCE_COMPILER="../../../../build/tmp-linux-x86_64/frida-core/tools/resource-compiler --toolchain=gnu" \
			AGENT=../../../../build/tmp-$*-stripped/frida-core/lib/agent/.libs/libfrida-agent.so \
		&& make install-data-am
	@touch -c $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core-32: check-core-linux-i386 ##@core Run tests for i386
check-core-64: check-core-linux-x86_64 ##@core Run tests for x86-64
check-core-linux-%: build/tmp-linux-%/frida-core/tests/frida-tests
	$<


android-server-i386: build/frida-android-i386-stripped/bin/frida-server ##@android Build frida-server for Android/i386
android-server-arm: build/frida-android-arm-stripped/bin/frida-server ##@android Build frida-server for Android/ARM

build/frida-android-i386-stripped/bin/frida-server: build/frida-android-i386/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-i386.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/frida-android-arm-stripped/bin/frida-server: build/frida-android-arm/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-arm.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/frida-%/bin/frida-server: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/server/server.c,build/tmp-$*/frida-core/server/frida_server-server.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/server install
	@touch -c $@


python-32: build/frida-linux-i386-stripped/lib/$(PYTHON_NAME)/site-packages/frida build/frida-linux-i386-stripped/lib/$(PYTHON_NAME)/site-packages/_frida.so ##@bindings Build Python bindings for i386
python-64: build/frida-linux-x86_64-stripped/lib/$(PYTHON_NAME)/site-packages/frida build/frida-linux-x86_64-stripped/lib/$(PYTHON_NAME)/site-packages/_frida.so ##@bindings Build Python bindings for x86-64

frida-python/configure: build/frida-env-linux-x86_64.rc frida-python/configure.ac
	. build/frida-env-linux-x86_64.rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-$(PYTHON_NAME)/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && PYTHON=$(PYTHON) ../../../frida-python/configure

build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la: build/tmp-%/frida-$(PYTHON_NAME)/Makefile build/frida-python-submodule-stamp
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-$(PYTHON_NAME)/src/_frida.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make install
	@touch -c $@

build/frida-%-stripped/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-$*/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@

build/frida-%-stripped/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-$(PYTHON_NAME)/src/.libs/_frida.so $@
	strip --strip-all $@

check-python-32: check-python-i386 ##@bindings Test Python bindings for i386
check-python-64: check-python-x86_64 ##@bindings Test Python bindings for x86-64

check-python-%: build/frida-%-stripped/lib/$(PYTHON_NAME)/site-packages/frida build/frida-%-stripped/lib/$(PYTHON_NAME)/site-packages/_frida.so
	export PYTHONPATH="$(shell pwd)/build/frida-linux-$*-stripped/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest tests.test_core tests.test_tracer


node-32: build/frida-linux-i386-stripped/lib/node_modules/frida ##@bindings Build Node.js bindings for i386
node-64: build/frida-linux-x86_64-stripped/lib/node_modules/frida ##@bindings Build Node.js bindings for x86-64

build/frida-%-stripped/lib/node_modules/frida: build/frida-%/lib/pkgconfig/frida-core-1.0.pc build/frida-node-submodule-stamp
	export PATH=$(NODE_BIN_DIR):$$PATH FRIDA=$(FRIDA) \
		&& cd frida-node \
		&& rm -rf frida-0.0.0.tgz build lib/binding node_modules \
		&& $(NPM) install --build-from-source \
		&& $(NPM) pack \
		&& rm -rf ../$@/ ../$@.tmp/ \
		&& mkdir -p ../$@.tmp/ \
		&& tar -C ../$@.tmp/ --strip-components 1 -x -f frida-0.0.0.tgz \
		&& mv lib/binding ../$@.tmp/lib/ \
		&& strip --strip-all ../$@.tmp/lib/binding/Release/node-*/frida_binding.node \
		&& mv ../$@.tmp ../$@

check-node-32: check-node-i386 ##@bindings Test Node.js bindings for i386
check-node-64: check-node-x86_64 ##@bindings Test Node.js bindings for x86-64

check-node-%: build/frida-%-stripped/lib/node_modules/frida
	$(NODE) --expose-gc $</node_modules/mocha/bin/_mocha


.PHONY: \
	help \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-32 capstone-64 capstone-update-submodule-stamp \
	gum-32 gum-64 check-gum-32 check-gum-64 check-gum-linux-i386 check-gum-linux-x86_64 frida-gum-update-submodule-stamp \
	core-32 core-64 check-core-32 check-core-64 check-core-linux-i386 check-core-linux-x86_64 frida-core-update-submodule-stamp \
	android-server-i386 android-server-arm \
	python-32 python-64 check-python-32 check-python-64 check-python-i386 check-python-x86_64 frida-python-update-submodule-stamp \
	node-32 node-64 check-node-32 check-node-64 check-node-i386 check-node-x86_64 frida-node-update-submodule-stamp
.SECONDARY:
