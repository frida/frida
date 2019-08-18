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
	print "  \$$ make $${target_color}python-linux-x86_64 $${variable_color}PYTHON$${reset_color}=/opt/python36-64/bin/python3.6\n"; \
	print "  \$$ make $${target_color}node-linux-x86 $${variable_color}NODE$${reset_color}=/opt/node-linux-x86/bin/node\n"; \
	print "\n";

help:
	@LC_ALL=C perl -e '$(HELP_FUN)' $(MAKEFILE_LIST)


include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/.*-gum-npm-stamp
	rm -f build/*-clang*
	rm -f build/*-pkg-config
	rm -f build/*-stamp
	rm -f build/*-strip
	rm -f build/*.rc
	rm -f build/*.sh
	rm -f build/*.site
	rm -f build/*.txt
	rm -f build/frida-version.h
	rm -rf build/frida-*-*
	rm -rf build/frida_thin-*-*
	rm -rf build/fs-*-*
	rm -rf build/ft-*-*
	rm -rf build/tmp-*-*
	rm -rf build/tmp_thin-*-*
	rm -rf build/fs-tmp-*-*
	rm -rf build/ft-tmp-*-*

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd
	cd frida-tools && git clean -xfd


define make-capstone-rule
build/$1-%/lib/pkgconfig/capstone.pc: build/$1-env-%.rc build/.capstone-submodule-stamp
	. build/$1-env-$$*.rc \
		&& export PACKAGE_TARNAME=capstone \
		&& . $$$$CONFIG_SITE \
		&& case $$* in \
			*-x86)       capstone_archs="x86"     ;; \
			*-x86_64)    capstone_archs="x86"     ;; \
			*-arm)       capstone_archs="arm"     ;; \
			*-armhf)     capstone_archs="arm"     ;; \
			*-armeabi)   capstone_archs="arm"     ;; \
			*-arm64)     capstone_archs="aarch64" ;; \
			*-mips)      capstone_archs="mips"    ;; \
			*-mipsel)    capstone_archs="mips"    ;; \
			*-mips64)    capstone_archs="mips64"    ;; \
			*-mips64el)  capstone_archs="mips64"    ;; \
		esac \
		&& CFLAGS="$$$$CPPFLAGS $$$$CFLAGS" make -C capstone \
			PREFIX=$$$$frida_prefix \
			BUILDDIR=../build/$2-$$*/capstone \
			CAPSTONE_BUILD_CORE_ONLY=yes \
			CAPSTONE_ARCHS="$$$$capstone_archs" \
			CAPSTONE_SHARED=$$$$enable_shared \
			CAPSTONE_STATIC=$$$$enable_static \
			install
endef
$(eval $(call make-capstone-rule,frida,tmp))
$(eval $(call make-capstone-rule,frida_thin,tmp_thin))


gum-linux-x86: build/frida-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/x86
gum-linux-x86_64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/x86-64
gum-linux-x86-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/x86 without cross-arch support
gum-linux-x86_64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/x86-64 without cross-arch support
gum-linux-arm: build/frida_thin-linux-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/ARM
gum-linux-armhf: build/frida_thin-linux-armhf/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/ARMhf
gum-linux-arm64: build/frida_thin-linux-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/ARM64
gum-linux-mips: build/frida_thin-linux-mips/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/MIPS
gum-linux-mipsel: build/frida_thin-linux-mipsel/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/MIPSel
gum-linux-mips64: build/frida_thin-linux-mips64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/MIPS64
gum-linux-mips64el: build/frida_thin-linux-mips64el/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Linux/MIP64Sel
gum-android-x86: build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86
gum-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86-64
gum-android-arm: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/ARM
gum-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/ARM64
gum-qnx-arm: build/frida_thin-qnx-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for QNX/ARM
gum-qnx-armeabi: build/frida_thin-qnx-armeabi/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for QNX/ARMEABI


define make-gum-rules
build/.$1-gum-npm-stamp: build/$1-env-linux-$$(build_arch).rc
	@$$(NPM) --version &>/dev/null || (echo -e "\033[31mOops. It appears Node.js is not installed.\nWe need it for processing JavaScript code at build-time.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	. build/$1-env-linux-$$(build_arch).rc && cd frida-gum/bindings/gumjs && $(NPM) install
	@touch $$@

build/$1-%/lib/pkgconfig/frida-gum-1.0.pc: build/.frida-gum-submodule-stamp build/.$1-gum-npm-stamp build/$1-%/lib/pkgconfig/capstone.pc
	. build/$1-meson-env-linux-$$(build_arch).rc; \
	builddir=build/$2-$$*/frida-gum; \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		mkdir -p $$$$builddir; \
		if [ $$(build_platform_arch) = $$* ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/$1-$$*.txt"; \
		fi; \
		$$(MESON) \
			--prefix $$(FRIDA)/build/$1-$$* \
			--libdir $$(FRIDA)/build/$1-$$*/lib \
			$$$$cross_args \
			$$(frida_gum_flags) \
			frida-gum $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch -c $$@
endef
$(eval $(call make-gum-rules,frida,tmp))
$(eval $(call make-gum-rules,frida_thin,tmp_thin))

check-gum-linux-x86: gum-linux-x86 ##@gum Run tests for Linux/x86
	build/tmp-linux-x86/frida-gum/tests/gum-tests $(test_args)
check-gum-linux-x86_64: gum-linux-x86_64 ##@gum Run tests for Linux/x86-64
	build/tmp-linux-x86_64/frida-gum/tests/gum-tests $(test_args)
check-gum-linux-x86-thin: gum-linux-x86-thin ##@gum Run tests for Linux/x86 without cross-arch support
	build/tmp_thin-linux-x86/frida-gum/tests/gum-tests $(test_args)
check-gum-linux-x86_64-thin: gum-linux-x86_64-thin ##@gum Run tests for Linux/x86-64 without cross-arch support
	build/tmp_thin-linux-x86_64/frida-gum/tests/gum-tests $(test_args)
check-gum-linux-arm64: gum-linux-arm64 ##@gum Run tests for Linux/ARM64
	build/tmp_thin-linux-arm64/frida-gum/tests/gum-tests $(test_args)


core-linux-x86: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/x86
core-linux-x86_64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/x86-64
core-linux-x86-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/x86 without cross-arch support
core-linux-x86_64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/x86-64 without cross-arch support
core-linux-arm: build/frida_thin-linux-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/ARM
core-linux-armhf: build/frida_thin-linux-armhf/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/ARMhf
core-linux-arm64: build/frida_thin-linux-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/ARM64
core-linux-mips: build/frida_thin-linux-mips/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/MIPS
core-linux-mipsel: build/frida_thin-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/MIPSel
core-linux-mips64: build/frida_thin-linux-mips64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/MIPS64
core-linux-mips64el: build/frida_thin-linux-mips64el/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Linux/MIPS64el
core-android-x86: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86
core-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86-64
core-android-arm: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/ARM
core-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/ARM64
core-qnx-arm: build/frida_thin-qnx-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX/ARM
core-qnx-armeabi: build/frida_thin-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX/ARMEABI

build/tmp-linux-x86/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-x86/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		if [ $(build_arch) = x86 ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/frida-linux-x86.txt"; \
		fi; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-x86 \
			--libdir $(FRIDA)/build/frida-linux-x86/lib \
			$$cross_args \
			$(frida_core_flags) \
			-Dhelper32=$(FRIDA)/build/tmp-linux-x86/frida-core/src/frida-helper \
			-Dhelper64=$(FRIDA)/build/tmp-linux-x86_64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-linux-x86/frida-core/lib/agent/frida-agent.so \
			-Dagent64=$(FRIDA)/build/tmp-linux-x86_64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-linux-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		if [ $(build_arch) = x86_64 ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/frida-linux-x86_64.txt"; \
		fi; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-x86_64 \
			--libdir $(FRIDA)/build/frida-linux-x86_64/lib \
			$$cross_args \
			$(frida_core_flags) \
			-Dhelper32=$(FRIDA)/build/tmp-linux-x86/frida-core/src/frida-helper \
			-Dhelper64=$(FRIDA)/build/tmp-linux-x86_64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-linux-x86/frida-core/lib/agent/frida-agent.so \
			-Dagent64=$(FRIDA)/build/tmp-linux-x86_64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-x86 \
			--libdir $(FRIDA)/build/frida-android-x86/lib \
			--cross-file build/frida-android-x86.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-x86_64 \
			--libdir $(FRIDA)/build/frida-android-x86_64/lib \
			--cross-file build/frida-android-x86_64.txt \
			$(frida_core_flags) \
			-Dhelper32=$(FRIDA)/build/tmp-android-x86/frida-core/src/frida-helper \
			-Dhelper64=$(FRIDA)/build/tmp-android-x86_64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-android-x86/frida-core/lib/agent/frida-agent.so \
			-Dagent64=$(FRIDA)/build/tmp-android-x86_64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-arm/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-arm \
			--libdir $(FRIDA)/build/frida-android-arm/lib \
			--cross-file build/frida-android-arm.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-arm64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-arm64 \
			--libdir $(FRIDA)/build/frida-android-arm64/lib \
			--cross-file build/frida-android-arm64.txt \
			$(frida_core_flags) \
			-Dhelper32=$(FRIDA)/build/tmp-android-arm/frida-core/src/frida-helper \
			-Dhelper64=$(FRIDA)/build/tmp-android-arm64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so \
			-Dagent64=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp_thin-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida_thin-%/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida_thin-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		if [ $(build_platform_arch) = $* ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/frida_thin-$*.txt"; \
		fi; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida_thin-$* \
			--libdir $(FRIDA)/build/frida_thin-$*/lib \
			$$cross_args \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@

build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-linux-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-linux-x86/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-x86/frida-core install
	@touch $@
build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-linux-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-linux-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-x86_64/frida-core install
	@touch $@
build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-x86/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86/frida-core install
	@touch $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-android-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86_64/frida-core install
	@touch $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm/frida-core install
	@touch $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp build/tmp-android-arm64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm64/frida-core install
	@touch $@
build/frida_thin-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_thin-%/frida-core/.frida-ninja-stamp
	. build/frida_thin-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp_thin-$*/frida-core install
	@touch $@

build/tmp-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-$*/frida-core src/frida-helper lib/agent/frida-agent.so
	@touch $@

check-core-linux-x86: core-linux-x86 ##@core Run tests for Linux/x86
	build/tmp-linux-x86/frida-core/tests/frida-tests $(test_args)
check-core-linux-x86_64: core-linux-x86_64 ##@core Run tests for Linux/x86-64
	build/tmp-linux-x86_64/frida-core/tests/frida-tests $(test_args)
check-core-linux-x86-thin: core-linux-x86-thin ##@core Run tests for Linux/x86 without cross-arch support
	build/tmp_thin-linux-x86/frida-core/tests/frida-tests $(test_args)
check-core-linux-x86_64-thin: core-linux-x86_64-thin ##@core Run tests for Linux/x86-64 without cross-arch support
	build/tmp_thin-linux-x86_64/frida-core/tests/frida-tests $(test_args)
check-core-linux-arm64: core-linux-arm64 ##@core Run tests for Linux/ARM64
	build/tmp_thin-linux-arm64/frida-core/tests/frida-tests $(test_args)


python-linux-x86: build/tmp-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for Linux/x86
python-linux-x86_64: build/tmp-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for Linux/x86-64
python-linux-x86-thin: build/tmp_thin-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for Linux/x86 without cross-arch support
python-linux-x86_64-thin: build/tmp_thin-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for Linux/x86-64 without cross-arch support
python-linux-arm64: build/tmp_thin-linux-arm64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for Linux/ARM64

define make-python-rule
build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp: build/.frida-python-submodule-stamp build/$1-%/lib/pkgconfig/frida-core-1.0.pc
	. build/$1-meson-env-linux-$$(build_arch).rc; \
	builddir=$$(@D); \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		mkdir -p $$$$builddir; \
		if [ $$(build_platform_arch) = $$* ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/$1-$$*.txt"; \
		fi; \
		$$(MESON) \
			--prefix $$(FRIDA)/build/$1-$$* \
			--libdir $$(FRIDA)/build/$1-$$*/lib \
			$$$$cross_args \
			-Dpython=$$(PYTHON) \
			-Dpython_incdir=$$(PYTHON_INCDIR) \
			frida-python $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch $$@
endef
$(eval $(call make-python-rule,frida,tmp))
$(eval $(call make-python-rule,frida_thin,tmp_thin))

check-python-linux-x86: build/tmp-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for Linux/x86
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-linux-x86_64: build/tmp-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for Linux/x86-64
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-linux-x86-thin: build/tmp_thin-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for Linux/x86 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-linux-x86_64-thin: build/tmp_thin-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for Linux/x86-64 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-linux-arm64: build/tmp_thin-linux-arm64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for Linux/ARM64
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-arm64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover


node-linux-x86: build/frida-linux-x86/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for Linux/x86
node-linux-x86_64: build/frida-linux-x86_64/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for Linux/x86-64
node-linux-x86-thin: build/frida_thin-linux-x86/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for Linux/x86 without cross-arch support
node-linux-x86_64-thin: build/frida_thin-linux-x86_64/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for Linux/x86-64 without cross-arch support
node-linux-arm64: build/frida_thin-linux-arm64/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for Linux/ARM64

define make-node-rule
build/$1-%/lib/node_modules/frida: build/$1-%/lib/pkgconfig/frida-core-1.0.pc build/.frida-node-submodule-stamp
	@$$(NPM) --version &>/dev/null || (echo -e "\033[31mOops. It appears Node.js is not installed.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	export PATH=$$(NODE_BIN_DIR):$$$$PATH FRIDA=$$(FRIDA) \
		&& cd frida-node \
		&& rm -rf frida-0.0.0.tgz build node_modules \
		&& $$(NPM) install \
		&& $$(NPM) pack \
		&& rm -rf ../$$@/ ../$$@.tmp/ \
		&& mkdir -p ../$$@.tmp/build/ \
		&& tar -C ../$$@.tmp/ --strip-components 1 -x -f frida-0.0.0.tgz \
		&& rm frida-0.0.0.tgz \
		&& mv build/Release/frida_binding.node ../$$@.tmp/build/ \
		&& rm -rf build \
		&& mv node_modules ../$$@.tmp/ \
		&& strip --strip-all ../$$@.tmp/build/frida_binding.node \
		&& mv ../$$@.tmp ../$$@
endef
$(eval $(call make-node-rule,frida,tmp))
$(eval $(call make-node-rule,frida_thin,tmp_thin))

define run-node-tests
	export PATH=$3:$$PATH FRIDA=$2 \
		&& cd frida-node \
		&& git clean -xfd \
		&& $5 install \
		&& $4 \
			--expose-gc \
			../build/$1/lib/node_modules/frida/node_modules/.bin/_mocha \
			-r ts-node/register \
			--timeout 60000 \
			test/*.ts
endef
check-node-linux-x86: node-linux-x86 ##@node Test Node.js bindings for Linux/x86
	$(call run-node-tests,frida-linux-x86,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-linux-x86_64: node-linux-x86_64 ##@node Test Node.js bindings for Linux/x86-64
	$(call run-node-tests,frida-linux-x86_64,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-linux-x86-thin: node-linux-x86-thin ##@node Test Node.js bindings for Linux/x86 without cross-arch support
	$(call run-node-tests,frida_thin-linux-x86,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-linux-x86_64-thin: node-linux-x86_64-thin ##@node Test Node.js bindings for Linux/x86-64 without cross-arch support
	$(call run-node-tests,frida_thin-linux-x86_64,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-linux-arm64: node-linux-arm64 ##@node Test Node.js bindings for Linux/ARM64
	$(call run-node-tests,frida_thin-linux-arm64,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))


tools-linux-x86: build/tmp-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for Linux/x86
tools-linux-x86_64: build/tmp-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for Linux/x86-64
tools-linux-x86-thin: build/tmp_thin-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for Linux/x86 without cross-arch support
tools-linux-x86_64-thin: build/tmp_thin-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for Linux/x86-64 without cross-arch support
tools-linux-arm64: build/tmp_thin-linux-arm64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for Linux/ARM64

define make-tools-rule
build/$2-%/frida-tools-$$(PYTHON_NAME)/.frida-stamp: build/.frida-tools-submodule-stamp build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp
	. build/$1-meson-env-linux-$$(build_arch).rc; \
	builddir=$$(@D); \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		mkdir -p $$$$builddir; \
		if [ $$(build_platform_arch) = $$* ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/$1-$$*.txt"; \
		fi; \
		$$(MESON) \
			--prefix $$(FRIDA)/build/$1-$$* \
			--libdir $$(FRIDA)/build/$1-$$*/lib \
			$$$$cross_args \
			-Dpython=$$(PYTHON) \
			frida-tools $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch $$@
endef
$(eval $(call make-tools-rule,frida,tmp))
$(eval $(call make-tools-rule,frida_thin,tmp_thin))

check-tools-linux-x86: build/tmp-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for Linux/x86
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-linux-x86_64: build/tmp-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for Linux/x86-64
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-linux-x86-thin: build/tmp_thin-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for Linux/x86 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-linux-x86_64-thin: build/tmp_thin-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for Linux/x86-64 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-linux-arm64: build/tmp_thin-linux-arm64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for Linux/ARM64
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-arm64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover


.PHONY: \
	help \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-update-submodule-stamp \
	gum-linux-x86 gum-linux-x86_64 \
		gum-linux-x86-thin gum-linux-x86_64-thin \
		gum-linux-arm gum-linux-armhf gum-linux-arm64 \
		gum-linux-mips gum-linux-mipsel \
		gum-linux-mips64 gum-linux-mips64el \
		gum-android-x86 gum-android-x86_64 \
		gum-android-arm gum-android-arm64 \
		gum-qnx-arm gum-qnx-armeabi \
		check-gum-linux-x86 check-gum-linux-x86_64 \
		check-gum-linux-x86-thin check-gum-linux-x86_64-thin \
		check-gum-linux-arm64 \
		frida-gum-update-submodule-stamp \
	core-linux-x86 core-linux-x86_64 \
		core-linux-x86-thin core-linux-x86_64-thin \
		core-linux-arm core-linux-armhf core-linux-arm64 \
		core-linux-mips core-linux-mipsel \
		core-linux-mips64 core-linux-mips64el \
		core-android-x86 core-android-x86_64 \
		core-android-arm core-android-arm64 \
		core-qnx-arm core-qnx-armeabi \
		check-core-linux-x86 check-core-linux-x86_64 \
		check-core-linux-x86-thin check-core-linux-x86_64-thin \
		check-core-linux-arm64 \
		frida-core-update-submodule-stamp \
	python-linux-x86 python-linux-x86_64 \
		python-linux-x86-thin python-linux-x86_64-thin \
		python-linux-arm64 \
		check-python-linux-x86 check-python-linux-x86_64 \
		check-python-linux-x86-thin check-python-linux-x86_64-thin \
		check-python-linux-arm64 \
		frida-python-update-submodule-stamp \
	node-linux-x86 node-linux-x86_64 \
		node-linux-x86-thin node-linux-x86_64-thin \
		node-linux-arm64 \
		check-node-linux-x86 check-node-linux-x86_64 \
		check-node-linux-x86-thin check-node-linux-x86_64-thin \
		check-node-linux-arm64 \
		frida-node-update-submodule-stamp \
	tools-linux-x86 tools-linux-x86_64 \
		tools-linux-x86-thin tools-linux-x86_64-thin \
		tools-linux-arm64 \
		check-tools-linux-x86 check-tools-linux-x86_64 \
		check-tools-linux-x86-thin check-tools-linux-x86_64-thin \
		check-tools-linux-arm64 \
		frida-tools-update-submodule-stamp \
	glib glib-symlinks \
	v8 v8-symlinks
.SECONDARY:
