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
		print "  /* $$_ */\n"; $$sep = " " x (20 - length $$_->[0]); \
		printf("  $${target_color}%-20s$${reset_color}    %s\n", $$_->[0], $$_->[1]) for @{$$help{$$_}}; \
		print "\n"; \
	} \
	print "And optionally also $${variable_color}VARIABLE$${reset_color} values:\n"; \
	print "  $${variable_color}PYTHON$${reset_color}                  Absolute path of Python interpreter including version suffix\n"; \
	print "  $${variable_color}NODE$${reset_color}                    Absolute path of Node.js binary\n"; \
	print "\n"; \
	print "For example:\n"; \
	print "  \$$ make $${target_color}python-64 $${variable_color}PYTHON$${reset_color}=/opt/python36-64/bin/python3.6\n"; \
	print "  \$$ make $${target_color}node-32 $${variable_color}NODE$${reset_color}=/opt/node-32/bin/node\n"; \
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
			*-x86)    capstone_archs="x86"     ;; \
			*-x86_64) capstone_archs="x86"     ;; \
			*-arm)    capstone_archs="arm"     ;; \
			*-armhf)  capstone_archs="arm"     ;; \
			*-armeabi)capstone_archs="arm"     ;; \
			*-arm64)  capstone_archs="aarch64" ;; \
			*-mips)   capstone_archs="mips"    ;; \
			*-mipsel) capstone_archs="mips"    ;; \
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


gum-32: build/frida-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86
gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86-64
gum-32-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86 without cross-arch support
gum-64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86-64 without cross-arch support
gum-android: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android


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

check-gum-32: build/frida-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for x86
	build/tmp-linux-x86/frida-gum/tests/gum-tests $(test_args)
check-gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for x86-64
	build/tmp-linux-x86_64/frida-gum/tests/gum-tests $(test_args)
check-gum-32-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for x86 without cross-arch support
	build/tmp_thin-linux-x86/frida-gum/tests/gum-tests $(test_args)
check-gum-64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for x86-64 without cross-arch support
	build/tmp_thin-linux-x86_64/frida-gum/tests/gum-tests $(test_args)


core-32: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86
core-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86-64
core-32-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86 without cross-arch support
core-64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86-64 without cross-arch support
core-android: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android
core-qnx-arm: build/frida-qnx-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX-arm
core-qnx-armeabi: build/frida-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX-armeabi
core-linux-mips: build/frida-linux-mips/lib/pkgconfig/frida-core-1.0.pc ##@core Build for mips
core-linux-mipsel: build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc ##@core Build for mipsel

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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-linux-x86/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-linux-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-linux-x86/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-linux-x86_64/frida-core/lib/agent/frida-agent.so \
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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-linux-x86/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-linux-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-linux-x86/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-linux-x86_64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-linux-arm/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-arm/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-arm \
			--libdir $(FRIDA)/build/frida-linux-arm/lib \
			--cross-file build/frida-linux-arm.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-linux-armhf/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-armhf/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-armhf \
			--libdir $(FRIDA)/build/frida-linux-armhf/lib \
			--cross-file build/frida-linux-armhf.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-linux-mips/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-mips/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-mips \
			--libdir $(FRIDA)/build/frida-linux-mips/lib \
			--cross-file build/frida-linux-mips.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-linux-mipsel/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-linux-mipsel/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-linux-mipsel \
			--libdir $(FRIDA)/build/frida-linux-mipsel/lib \
			--cross-file build/frida-linux-mipsel.txt \
			$(frida_core_flags) \
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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-android-x86/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-android-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-android-x86/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-android-x86_64/frida-core/lib/agent/frida-agent.so \
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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-android-arm/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-android-arm64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-qnx-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-qnx-%/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-linux-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-qnx-$* \
			--libdir $(FRIDA)/build/frida-qnx-$*/lib \
			--cross-file build/frida-qnx-$*.txt \
			$(frida_core_flags) \
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
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-x86/frida-core install
	@touch $@
build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-linux-x86_64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-x86_64/frida-core install
	@touch $@
build/frida-linux-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-arm/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-arm/frida-core install
	@touch $@
build/frida-linux-armhf/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-armhf/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-armhf/frida-core install
	@touch $@
build/frida-linux-mips/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-mips/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-mips/frida-core install
	@touch $@
build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc: build/tmp-linux-mipsel/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-linux-mipsel/frida-core install
	@touch $@
build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86/frida-core install
	@touch $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-android-x86_64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86_64/frida-core install
	@touch $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm/frida-core install
	@touch $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp build/tmp-android-arm64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm64/frida-core install
	@touch $@
build/frida-qnx-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-qnx-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-qnx-$*/frida-core install
	@touch $@
build/frida_thin-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_thin-%/frida-core/.frida-ninja-stamp
	. build/frida_thin-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp_thin-$*/frida-core install
	@touch $@

build/tmp-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-linux-$(build_arch).rc && $(NINJA) -C build/tmp-$*/frida-core src/frida-helper lib/agent/frida-agent.so
	@touch $@

check-core-32: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for x86
	build/tmp-linux-x86/frida-core/tests/frida-tests $(test_args)
check-core-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for x86-64
	build/tmp-linux-x86_64/frida-core/tests/frida-tests $(test_args)
check-core-32-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for x86 without cross-arch support
	build/tmp_thin-linux-x86/frida-core/tests/frida-tests $(test_args)
check-core-64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for x86-64 without cross-arch support
	build/tmp_thin-linux-x86_64/frida-core/tests/frida-tests $(test_args)

server-32: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@server Build for x86
server-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for x86-64
server-32-thin: build/frida_thin-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@server Build for x86 without cross-arch support
server-64-thin: build/frida_thin-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for x86-64 without cross-arch support
server-arm: build/frida-linux-arm/lib/pkgconfig/frida-core-1.0.pc ##@server Build for arm
server-armhf: build/frida-linux-armhf/lib/pkgconfig/frida-core-1.0.pc ##@server Build for arm
server-mips: build/frida-linux-mips/lib/pkgconfig/frida-core-1.0.pc ##@server Build for mips
server-mipsel: build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc ##@server Build for mipsel
server-android: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for Android
server-qnx-arm: build/frida-qnx-arm/lib/pkgconfig/frida-core-1.0.pc ##@server Build for QNX-arm
server-qnx-armeabi: build/frida-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc ##@server Build for QNX-armeabi

inject-32: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@inject Build for x86
inject-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@inject Build for x86-64
inject-arm: build/frida-linux-arm/lib/pkgconfig/frida-core-1.0.pc ##@inject Build for arm
inject-armhf: build/frida-linux-armhf/lib/pkgconfig/frida-core-1.0.pc ##@inject Build for armhf

gadget-32: build/frida-linux-x86/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for x86
gadget-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for x86-64
gadget-android: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for Android
gadget-arm: build/frida-linux-arm/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for linux-arm
gadget-armhf: build/frida-linux-armhf/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for linux-armhf
gadget-mipsel: build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for mipsel
gadget-qnx-arm: build/frida-qnx-arm/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for qnx-arm
gadget-qnx-armeabi: build/frida-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for qnx-armeabi


python-32: build/tmp-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for x86
python-64: build/tmp-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for x86-64
python-32-thin: build/tmp_thin-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for x86 without cross-arch support
python-64-thin: build/tmp_thin-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for x86-64 without cross-arch support

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
			-Dwith-python=$$(PYTHON) \
			frida-python $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch $$@
endef
$(eval $(call make-python-rule,frida,tmp))
$(eval $(call make-python-rule,frida_thin,tmp_thin))

check-python-32: build/tmp-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for x86
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-64: build/tmp-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for x86-64
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-32-thin: build/tmp_thin-linux-x86/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for x86 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover
check-python-64-thin: build/tmp_thin-linux-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Test Python bindings for x86-64 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& ${PYTHON} -m unittest discover


node-32: build/frida-linux-x86/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for x86
node-64: build/frida-linux-x86_64/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for x86-64
node-32-thin: build/frida_thin-linux-x86/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for x86 without cross-arch support
node-64-thin: build/frida_thin-linux-x86_64/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for x86-64 without cross-arch support

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
check-node-32: node-32 ##@node Test Node.js bindings for x86
	$(call run-node-tests,frida-linux-x86,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-64: node-64 ##@node Test Node.js bindings for x86-64
	$(call run-node-tests,frida-linux-x86_64,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-32-thin: node-32-thin ##@node Test Node.js bindings for x86 without cross-arch support
	$(call run-node-tests,frida_thin-linux-x86,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-64-thin: node-64-thin ##@node Test Node.js bindings for x86-64 without cross-arch support
	$(call run-node-tests,frida_thin-linux-x86_64,$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))


tools-32: build/tmp-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for x86
tools-64: build/tmp-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for x86-64
tools-32-thin: build/tmp_thin-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for x86 without cross-arch support
tools-64-thin: build/tmp_thin-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for x86-64 without cross-arch support

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
			-Dwith-python=$$(PYTHON) \
			frida-tools $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch $$@
endef
$(eval $(call make-tools-rule,frida,tmp))
$(eval $(call make-tools-rule,frida_thin,tmp_thin))

check-tools-32: build/tmp-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for x86
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-64: build/tmp-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for x86-64
	export PYTHONPATH="$(shell pwd)/build/frida-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-32-thin: build/tmp_thin-linux-x86/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for x86 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover
check-tools-64-thin: build/tmp_thin-linux-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Test CLI tools for x86-64 without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-linux-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& ${PYTHON} -m unittest discover


.PHONY: \
	help \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-update-submodule-stamp \
	gum-32 gum-64 gum-32-thin gum-64-thin gum-android check-gum-32 check-gum-64 check-gum-32-thin check-gum-64-thin frida-gum-update-submodule-stamp \
	core-32 core-64 core-32-thin core-64-thin core-android check-core-32 check-core-64 check-core-32-thin check-core-64-thin frida-core-update-submodule-stamp \
	server-32 server-64 server-32-thin server-64-thin server-android server-qnx-arm server-qnx-armeabi \
	python-32 python-64 python-32-thin python-64-thin check-python-32 check-python-64 check-python-32-thin check-python-64-thin frida-python-update-submodule-stamp \
	node-32 node-64 node-32-thin node-64-thin check-node-32 check-node-64 check-node-32-thin check-node-64-thin frida-node-update-submodule-stamp \
	tools-32 tools-64 tools-32-thin tools-64-thin check-tools-32 check-tools-64 check-tools-32-thin check-tools-64-thin frida-tools-update-submodule-stamp \
	glib glib-symlinks \
	v8 v8-symlinks
.SECONDARY:
