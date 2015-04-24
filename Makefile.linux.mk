FRIDA := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

PYTHON ?= $(shell which python)
PYTHON_VERSION := $(shell $(PYTHON) -c 'import sys; v = sys.version_info; print("{0}.{1}".format(v[0], v[1]))')
PYTHON_NAME ?= python$(PYTHON_VERSION)

NODE ?= $(shell which node)
NODE_BIN_DIR := $(shell dirname $(NODE) 2>/dev/null)
NPM ?= $(NODE_BIN_DIR)/npm

tests ?= /

build_arch := $(shell releng/detect-arch.sh)

HELP_FUN = \
	my (%help, @sections); \
	while(<>) { \
		if (/^([\w-]+)\s*:.*\#\#(?:@([\w-]+))?\s(.*)$$/) { \
			$$section = $$2 // 'options'; \
			push @sections, $$section unless exists $$help{$$section}; \
			push @{$$help{$$section}}, [$$1, $$3]; \
		} \
	} \
	$$target_color = "\033[32m"; \
	$$variable_color = "\033[36m"; \
	$$reset_color = "\033[0m"; \
	print "\n"; \
	print "\033[31mUsage:$${reset_color} make $${target_color}TARGET$${reset_color} [$${variable_color}VARIABLE$${reset_color}=value]\n\n"; \
	print "Where $${target_color}TARGET$${reset_color} specifies one or more of:\n"; \
	print "\n"; \
	for (@sections) { \
		print "  /* $$_ */\n"; $$sep = " " x (20 - length $$_->[0]); \
		printf("  $${target_color}%-20s$${reset_color}    %s\n", $$_->[0], $$_->[1]) for @{$$help{$$_}}; \
		print "\n"; \
	} \
	print "And optionally also $${variable_color}VARIABLE$${reset_color} values:\n"; \
	print "  $${variable_color}PYTHON$${reset_color}                  Absolute path of Python interpreter including version suffix\n"; \
	print "  $${variable_color}NODE$${reset_color}                    Absolute path of Node.js binary\n"; \
	print "\n"; \
	print "For example:\n"; \
	print "  \$$ make $${target_color}python-64 $${variable_color}PYTHON$${reset_color}=/opt/python34-64/bin/python3.4\n"; \
	print "  \$$ make $${target_color}node-32 $${variable_color}NODE$${reset_color}=/opt/node-32/bin/node\n"; \
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
	rm -rf build/frida-linux-x86_64
	rm -rf build/frida-android-i386
	rm -rf build/frida-android-arm
	rm -rf build/frida-qnx-i386
	rm -rf build/frida-qnx-arm
	rm -rf build/frida_stripped-linux-i386
	rm -rf build/frida_stripped-linux-x86_64
	rm -rf build/frida_stripped-android-i386
	rm -rf build/frida_stripped-android-arm
	rm -rf build/frida_stripped-qnx-i386
	rm -rf build/frida_stripped-qnx-arm
	rm -rf build/tmp-linux-i386
	rm -rf build/tmp-linux-x86_64
	rm -rf build/tmp-android-i386
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-qnx-i386
	rm -rf build/tmp-qnx-arm
	rm -rf build/tmp_stripped-linux-i386
	rm -rf build/tmp_stripped-linux-x86_64
	rm -rf build/tmp_stripped-android-i386
	rm -rf build/tmp_stripped-android-arm
	rm -rf build/tmp_stripped-qnx-i386
	rm -rf build/tmp_stripped-qnx-arm

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd


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
gum-android: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android

frida-gum/configure: build/frida-env-linux-$(build_arch).rc frida-gum/configure.ac
	. build/frida-env-linux-$(build_arch).rc && cd frida-gum && ./autogen.sh

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc frida-gum/configure build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum-32: build/frida-linux-i386/lib/pkgconfig/frida-gum-1.0.pc build/frida-gum-submodule-stamp ##@gum Run tests for i386
	build/tmp-linux-i386/frida-gum/tests/gum-tests -p $(tests)
check-gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc build/frida-gum-submodule-stamp ##@gum Run tests for x86-64
	build/tmp-linux-x86_64/frida-gum/tests/gum-tests -p $(tests)


core-32: build/frida-linux-i386/lib/pkgconfig/frida-core-1.0.pc ##@core Build for i386
core-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86-64
core-android: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android

frida-core/configure: build/frida-env-linux-$(build_arch).rc frida-core/configure.ac
	. build/frida-env-linux-$(build_arch).rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/agent/agent.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_la-agent.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib
	@touch -c $@
build/tmp_stripped-%/frida-core/lib/agent/.libs/libfrida-agent.so: build/tmp-%/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/agent/.libs/libfrida-agent.so $@
	. build/frida-env-$*.rc && $$STRIP --strip-all $@

build/tmp-%/frida-core/src/frida-helper: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/src/darwin/frida-helper-glue.c,build/tmp-$*/frida-core/src/frida-helper-glue.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/src libfrida-helper-types.la frida-helper
	@touch -c $@
build/tmp_stripped-%/frida-core/src/frida-helper: build/tmp-%/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/frida-linux-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-linux-i386/frida-core/src/frida-helper build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-linux-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-$*.rc \
		&& cd build/tmp-linux-$*/frida-core \
		&& make -C src install \
		 	RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			AGENT32=../../../../build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
			HELPER32=../../../../build/tmp_stripped-linux-i386/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper!frida-helper-64 \
		&& make install-data-am
	@touch -c $@
build/frida-android-i386/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-i386/frida-core/src/frida-helper
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-i386/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-i386.rc \
		&& cd build/tmp-android-i386/frida-core \
		&& make -C src install \
		 	RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			AGENT32=../../../../build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			HELPER32=../../../../build/tmp_stripped-android-i386/frida-core/src/frida-helper!frida-helper-32 \
		&& make install-data-am
	@touch -c $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-arm/frida-core/src/frida-helper
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-arm.rc \
		&& cd build/tmp-android-arm/frida-core \
		&& make -C src install \
		 	RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
		&& make install-data-am
	@touch -c $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core-32: build/tmp-linux-i386/frida-core/tests/frida-tests build/frida-core-submodule-stamp ##@core Run tests for i386
	$< -p $(tests)
check-core-64: build/tmp-linux-x86_64/frida-core/tests/frida-tests build/frida-core-submodule-stamp ##@core Run tests for x86-64
	$< -p $(tests)


server-32: build/frida_stripped-linux-i386/bin/frida-server ##@server Build for i386
server-64: build/frida_stripped-linux-x86_64/bin/frida-server ##@server Build for x86-64
server-android: build/frida_stripped-android-arm/bin/frida-server ##@server Build for Android

build/frida_stripped-%/bin/frida-server: build/frida-%/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida-%/bin/frida-server: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/server/server.c,build/tmp-$*/frida-core/server/frida_server-server.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/server install
	@touch -c $@


python-32: build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-python-submodule-stamp ##@python Build Python bindings for i386
python-64: build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-python-submodule-stamp ##@python Build Python bindings for x86-64

frida-python/configure: build/frida-env-linux-$(build_arch).rc frida-python/configure.ac
	. build/frida-env-linux-$(build_arch).rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-$(PYTHON_NAME)/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && PYTHON=$(PYTHON) ../../../frida-python/configure

build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la: build/tmp-%/frida-$(PYTHON_NAME)/Makefile build/frida-python-submodule-stamp
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-$(PYTHON_NAME)/src/_frida.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make install
	@touch -c $@

build/frida_stripped-%/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-$*/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida_stripped-%/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-$(PYTHON_NAME)/src/.libs/_frida.so $@
	strip --strip-all $@

check-python-32: build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/_frida.so ##@python Test Python bindings for i386
	export PYTHONPATH="$(shell pwd)/build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest tests.test_core tests.test_tracer
check-python-64: build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so ##@python Test Python bindings for x86-64
	export PYTHONPATH="$(shell pwd)/build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest tests.test_core tests.test_tracer


node-32: build/frida_stripped-linux-i386/lib/node_modules/frida build/frida-node-submodule-stamp ##@node Build Node.js bindings for i386
node-64: build/frida_stripped-linux-x86_64/lib/node_modules/frida build/frida-node-submodule-stamp ##@node Build Node.js bindings for x86-64

build/frida_stripped-%/lib/node_modules/frida: build/frida-%/lib/pkgconfig/frida-core-1.0.pc build/frida-node-submodule-stamp
	export PATH=$(NODE_BIN_DIR):$$PATH FRIDA=$(FRIDA) \
		&& cd frida-node \
		&& rm -rf frida-0.0.0.tgz build lib/binding node_modules \
		&& $(NPM) install --build-from-source \
		&& $(NPM) pack \
		&& rm -rf ../$@/ ../$@.tmp/ \
		&& mkdir -p ../$@.tmp/ \
		&& tar -C ../$@.tmp/ --strip-components 1 -x -f frida-0.0.0.tgz \
		&& rm frida-0.0.0.tgz \
		&& mv lib/binding ../$@.tmp/lib/ \
		&& mv node_modules ../$@.tmp/ \
		&& strip --strip-all ../$@.tmp/lib/binding/Release/node-*/frida_binding.node \
		&& mv ../$@.tmp ../$@

check-node-32: build/frida_stripped-linux-i386/lib/node_modules/frida ##@node Test Node.js bindings for i386
	cd $< && $(NODE) --expose-gc node_modules/mocha/bin/_mocha
check-node-64: build/frida_stripped-linux-x86_64/lib/node_modules/frida ##@node Test Node.js bindings for x86-64
	cd $< && $(NODE) --expose-gc node_modules/mocha/bin/_mocha


.PHONY: \
	help \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-update-submodule-stamp \
	gum-32 gum-64 gum-android check-gum-32 check-gum-64 frida-gum-update-submodule-stamp \
	core-32 core-64 core-android check-core-32 check-core-64 frida-core-update-submodule-stamp \
	server-32 server-64 server-android \
	python-32 python-64 check-python-32 check-python-64 frida-python-update-submodule-stamp \
	node-32 node-64 check-node-32 check-node-64 frida-node-update-submodule-stamp
.SECONDARY:
