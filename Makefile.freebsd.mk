include config.mk

build_arch := $(shell releng/detect-arch.sh)
test_args := $(addprefix -p=,$(tests))

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
		print "  /* $$_ */\n"; $$sep = " " x (30 - length $$_->[0]); \
		printf("  $${target_color}%-30s$${reset_color}    %s\n", $$_->[0], $$_->[1]) for @{$$help{$$_}}; \
		print "\n"; \
	} \
	print "And optionally also $${variable_color}VARIABLE$${reset_color} values:\n"; \
	print "  $${variable_color}PYTHON$${reset_color}                            Absolute path of Python interpreter including version suffix\n"; \
	print "  $${variable_color}NODE$${reset_color}                              Absolute path of Node.js binary\n"; \
	print "\n"; \
	print "For example:\n"; \
	print "  \$$ make $${target_color}python $${variable_color}PYTHON$${reset_color}=/opt/python36-64/bin/python3.6\n"; \
	print "  \$$ make $${target_color}node $${variable_color}NODE$${reset_color}=/opt/node-freebsd-x86/bin/node\n"; \
	print "\n";

help:
	@LC_ALL=C perl -e '$(HELP_FUN)' $(MAKEFILE_LIST)


include releng/frida.mk

distclean: clean-submodules
	rm -rf build/
	rm -rf deps/

clean: clean-submodules
	rm -f build/*-clang*
	rm -f build/*-pkg-config
	rm -f build/*-stamp
	rm -f build/*.rc
	rm -f build/*.tar.bz2
	rm -f build/*.txt
	rm -f build/frida-version.h
	rm -rf build/frida-*-*
	rm -rf build/fs-*-*
	rm -rf build/ft-*-*
	rm -rf build/tmp-*-*
	rm -rf build/fs-tmp-*-*
	rm -rf build/ft-tmp-*-*

clean-submodules:
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd
	cd frida-tools && git clean -xfd


gum: build/frida-freebsd-$(build_arch)/libdata/pkgconfig/frida-gum-1.0.pc ##@gum Build


build/frida-%/libdata/pkgconfig/frida-gum-1.0.pc: build/frida-env-%.rc build/.frida-gum-submodule-stamp
	. build/frida-env-$*.rc; \
	builddir=build/tmp-$*/frida-gum; \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,$*) \
			--prefix $(FRIDA)/build/frida-$* \
			$(frida_gum_flags) \
			frida-gum $$builddir || exit 1; \
	fi; \
	$(MESON) install -C $$builddir || exit 1
	@touch -c $@

check-gum: gum ##@gum Run tests
	build/tmp-freebsd-$(build_arch)/frida-gum/tests/gum-tests $(test_args)


core: build/frida-freebsd-$(build_arch)/libdata/pkgconfig/frida-core-1.0.pc ##@core Build

build/tmp-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-%/libdata/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-$*.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,$*) \
			--prefix $(FRIDA)/build/frida-$* \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@

build/frida-%/libdata/pkgconfig/frida-core-1.0.pc: build/tmp-%/frida-core/.frida-ninja-stamp
	. build/frida-env-$*.rc && $(MESON) install -C build/tmp-$*/frida-core
	@touch $@

check-core: core ##@core Run tests
	build/tmp-freebsd-$(build_arch)/frida-core/tests/frida-tests $(test_args)


python: build/tmp-freebsd-$(build_arch)/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings

build/tmp-%/frida-$(PYTHON_NAME)/.frida-stamp: build/.frida-python-submodule-stamp build/frida-%/libdata/pkgconfig/frida-core-1.0.pc
	. build/frida-env-$*.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,$*) \
			--prefix $(FRIDA)/build/frida-$* \
			$(FRIDA_FLAGS_COMMON) \
			-Dpython=$(PYTHON) \
			frida-python $$builddir || exit 1; \
	fi; \
	$(MESON) install -C $$builddir || exit 1
	@touch $@

check-python: build/tmp-freebsd-$(build_arch)/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings
	export PYTHONPATH="$(shell pwd)/build/frida-freebsd-$(build_arch)/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover


node: build/frida-freebsd-$(build_arch)/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings

build/frida-%/lib/node_modules/frida: build/frida-%/libdata/pkgconfig/frida-core-1.0.pc build/.frida-node-submodule-stamp
	@$(NPM) --version 1>/dev/null 2>&1 || (echo -e "\033[31mOops. It appears Node.js is not installed.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	export PATH=$(NODE_BIN_DIR):$$PATH FRIDA=$(FRIDA) \
		&& cd frida-node \
		&& rm -rf frida-0.0.0.tgz build node_modules \
		&& $(NPM) install \
		&& $(NPM) pack \
		&& rm -rf ../$@/ ../$@.tmp/ \
		&& mkdir -p ../$@.tmp/build/ \
		&& tar -C ../$@.tmp/ --strip-components 1 -x -f frida-0.0.0.tgz \
		&& rm frida-0.0.0.tgz \
		&& mv build/Release/frida_binding.node ../$@.tmp/build/ \
		&& rm -rf build \
		&& mv node_modules ../$@.tmp/ \
		&& strip --strip-all ../$@.tmp/build/frida_binding.node \
		&& mv ../$@.tmp ../$@

check-node: node ##@node Test Node.js bindings
	export PATH=$(NODE_BIN_DIR):$$PATH FRIDA=$(FRIDA) \
		&& cd frida-node \
		&& git clean -xfd \
		&& $(NPM) install \
		&& $(NODE) \
			--expose-gc \
			../build/frida-freebsd-$(build_arch)/lib/node_modules/frida/node_modules/.bin/_mocha \
			-r ts-node/register \
			--timeout 60000 \
			test/*.ts


tools: build/tmp-freebsd-$(build_arch)/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools

build/tmp-%/frida-tools-$(PYTHON_NAME)/.frida-stamp: build/.frida-tools-submodule-stamp build/tmp-%/frida-$(PYTHON_NAME)/.frida-stamp
	. build/frida-env-$*.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,$*) \
			--prefix $(FRIDA)/build/frida-$* \
			$(FRIDA_FLAGS_COMMON) \
			-Dpython=$(PYTHON) \
			frida-tools $$builddir || exit 1; \
	fi; \
	$(MESON) install -C $$builddir || exit 1
	@touch $@

check-tools: build/tmp-freebsd-$(build_arch)/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools
	export PYTHONPATH="$(shell pwd)/build/frida-freebsd-$(build_arch)/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover


.PHONY: \
	help \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	gum check-gum frida-gum-update-submodule-stamp \
	core check-core frida-core-update-submodule-stamp \
	python check-python frida-python-update-submodule-stamp \
	node check-node frida-node-update-submodule-stamp \
	tools check-tools frida-tools-update-submodule-stamp
.SECONDARY:
