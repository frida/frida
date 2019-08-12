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
		print "  /* $$_ */\n"; $$sep = " " x (23 - length $$_->[0]); \
		printf("  $${target_color}%-23s$${reset_color}    %s\n", $$_->[0], $$_->[1]) for @{$$help{$$_}}; \
		print "\n"; \
	} \
	print "And optionally also $${variable_color}VARIABLE$${reset_color} values:\n"; \
	print "  $${variable_color}PYTHON$${reset_color}                     Absolute path of Python interpreter including version suffix\n"; \
	print "  $${variable_color}NODE$${reset_color}                       Absolute path of Node.js binary\n"; \
	print "\n"; \
	print "For example:\n"; \
	print "  \$$ make $${target_color}python-macos $${variable_color}PYTHON$${reset_color}=/usr/local/bin/python3.6\n"; \
	print "  \$$ make $${target_color}node-macos $${variable_color}NODE$${reset_color}=/usr/local/bin/node\n"; \
	print "\n";

help:
	@LC_ALL=C perl -e '$(HELP_FUN)' $(MAKEFILE_LIST)


include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/.frida-gum-npm-stamp
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
		&& case $1-$$* in \
			*-x86)           capstone_archs="x86"         ;; \
			*-x86_64)        capstone_archs="x86"         ;; \
			*-arm)           capstone_archs="arm"         ;; \
			frida-ios-arm64) capstone_archs="aarch64 arm" ;; \
			*-arm64)         capstone_archs="aarch64"     ;; \
		esac \
		&& CFLAGS="$$$$CPPFLAGS $$$$CFLAGS" make -C capstone \
			PREFIX=$$$$frida_prefix \
			BUILDDIR=../build/$2-$$*/capstone \
			CAPSTONE_BUILD_CORE_ONLY=yes \
			CAPSTONE_ARCHS="$$$$capstone_archs" \
			CAPSTONE_SHARED=$$$$enable_shared \
			CAPSTONE_STATIC=$$$$enable_static \
			LIBARCHS="" \
			install
endef
$(eval $(call make-capstone-rule,frida,tmp))
$(eval $(call make-capstone-rule,frida_thin,tmp_thin))


gum-macos: build/frida-macos-x86/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for macOS
gum-macos-thin: build/frida_thin-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for macOS without cross-arch support
gum-ios: build/frida-ios-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for iOS
gum-ios-thin: build/frida_thin-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for iOS without cross-arch support
gum-android-x86: build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86
gum-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86-64
gum-android-arm: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/ARM
gum-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/ARM64

define make-gum-rules
build/.$1-gum-npm-stamp: build/$1-env-macos-$$(build_arch).rc
	@$$(NPM) --version &>/dev/null || (echo -e "\033[31mOops. It appears Node.js is not installed.\nWe need it for processing JavaScript code at build-time.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	. build/$1-env-macos-$$(build_arch).rc && cd frida-gum/bindings/gumjs && npm install
	@touch $$@

build/$1-%/lib/pkgconfig/frida-gum-1.0.pc: build/.frida-gum-submodule-stamp build/.$1-gum-npm-stamp build/$1-%/lib/pkgconfig/capstone.pc
	. build/$1-meson-env-macos-$$(build_arch).rc; \
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
			$$$$cross_args \
			$$(frida_gum_flags) \
			frida-gum $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch -c $$@
endef
$(eval $(call make-gum-rules,frida,tmp))
$(eval $(call make-gum-rules,frida_thin,tmp_thin))

check-gum-macos: build/frida-macos-x86/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for macOS
	build/tmp-macos-x86/frida-gum/tests/gum-tests $(test_args)
	build/tmp-macos-x86_64/frida-gum/tests/gum-tests $(test_args)
check-gum-macos-thin: build/frida_thin-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for macOS without cross-arch support
	build/tmp_thin-macos-x86_64/frida-gum/tests/gum-tests $(test_args)


core-macos: build/.core-macos-stamp-frida-macos-x86 build/.core-macos-stamp-frida-macos-x86_64 ##@core Build for macOS
core-macos-thin: build/.core-macos-stamp-frida_thin-macos-x86_64 ##@core Build for macOS without cross-arch support
core-ios: build/.core-ios-stamp-frida-ios-arm build/.core-ios-stamp-frida-ios-arm64 ##@core Build for iOS
core-ios-thin: build/.core-ios-stamp-frida_thin-ios-arm64 ##@core Build for iOS without cross-arch support
core-android-x86: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86
core-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86-64
core-android-arm: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/ARM
core-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/ARM64

build/tmp-macos-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-macos-%/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		if [ $(build_platform_arch) = macos-$* ]; then \
			cross_args=""; \
		else \
			cross_args="--cross-file build/frida-macos-$*.txt"; \
		fi; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-macos-$* \
			$$cross_args \
			$(frida_core_flags) \
			-Dhelper64=$(FRIDA)/build/tmp-macos-x86_64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-macos-x86/frida-core/lib/agent/frida-agent.dylib \
			-Dagent64=$(FRIDA)/build/tmp-macos-x86_64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-ios-x86/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-ios-x86/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-ios-x86 \
			--cross-file build/frida-ios-x86.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-ios-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-ios-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-ios-x86_64 \
			--cross-file build/frida-ios-x86_64.txt \
			$(frida_core_flags) \
			-Dhelper64=$(FRIDA)/build/tmp-ios-x86_64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-ios-x86/frida-core/lib/agent/frida-agent.dylib \
			-Dagent64=$(FRIDA)/build/tmp-ios-x86_64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-ios-arm/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-ios-arm/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-ios-arm \
			--cross-file build/frida-ios-arm.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-ios-arm64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-ios-arm64 \
			--cross-file build/frida-ios-arm64.txt \
			$(frida_core_flags) \
			-Dhelper64=$(FRIDA)/build/tmp-ios-arm64/frida-core/src/frida-helper \
			-Dagent32=$(FRIDA)/build/tmp-ios-arm/frida-core/lib/agent/frida-agent.dylib \
			-Dagent64=$(FRIDA)/build/tmp-ios-arm64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-x86 \
			--cross-file build/frida-android-x86.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-x86_64 \
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
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-arm \
			--cross-file build/frida-android-arm.txt \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-arm64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-android-arm64 \
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
	. build/frida_thin-meson-env-macos-$(build_arch).rc; \
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
			$$cross_args \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@

build/frida-macos-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-macos-x86/frida-core/.frida-agent-stamp build/tmp-macos-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-macos-$*/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-macos-$*/frida-core install
	@touch $@
build/frida-ios-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-x86/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-ios-x86/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-x86/frida-core install
	@touch $@
build/frida-ios-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-x86/frida-core/.frida-agent-stamp build/tmp-ios-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-ios-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-x86_64/frida-core install
	@touch $@
build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-arm/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-ios-arm/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-arm/frida-core install
	@touch $@
build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-arm/frida-core/.frida-agent-stamp build/tmp-ios-arm64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-ios-arm64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-arm64/frida-core install
	@touch $@
build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-x86/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86/frida-core install
	@touch $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-android-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86_64/frida-core install
	@touch $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm/frida-core install
	@touch $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp build/tmp-android-arm64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm64/frida-core install
	@touch $@
build/frida_thin-ios-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp_thin-ios-arm64/frida-core/.frida-ninja-stamp
	. build/frida_thin-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp_thin-ios-arm64/frida-core install
	@touch $@
build/frida_thin-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_thin-%/frida-core/.frida-ninja-stamp
	. build/frida_thin-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp_thin-$*/frida-core install
	@touch $@

build/tmp-macos-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-macos-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-macos-$*/frida-core src/frida-helper lib/agent/frida-agent.dylib
	@touch $@
build/tmp-macos-%/frida-core/.frida-agent-stamp: build/tmp-macos-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-macos-$*/frida-core lib/agent/frida-agent.dylib
	@touch $@
build/tmp-ios-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-ios-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-$*/frida-core src/frida-helper lib/agent/frida-agent.dylib
	@touch $@
build/tmp-ios-%/frida-core/.frida-agent-stamp: build/tmp-ios-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-$*/frida-core lib/agent/frida-agent.dylib
	@touch $@
build/tmp-android-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-android-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-$*/frida-core src/frida-helper lib/agent/frida-agent.so
	@touch $@

build/.core-macos-stamp-%: build/%/lib/pkgconfig/frida-core-1.0.pc
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#macos-and-ios"; exit 1; fi
	. build/frida-meson-env-macos-$(build_arch).rc \
		&& $$CODESIGN -f -s "$$MAC_CERTID" -i "re.frida.Server" build/$*/bin/frida-server \
		&& $$INSTALL_NAME_TOOL -id @executable_path/../Frameworks/FridaGadget.dylib build/$*/lib/frida-gadget.dylib \
		&& $$CODESIGN -f -s "$$MAC_CERTID" build/$*/lib/frida-gadget.dylib
	@touch $@
build/.core-ios-stamp-%: build/%/lib/pkgconfig/frida-core-1.0.pc
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#macos-and-ios"; exit 1; fi
	. build/frida-meson-env-macos-$(build_arch).rc \
		&& $$CODESIGN -f -s "$$IOS_CERTID" --entitlements frida-core/server/frida-server.xcent build/$*/bin/frida-server \
		&& $$INSTALL_NAME_TOOL -id @executable_path/Frameworks/FridaGadget.dylib build/$*/lib/frida-gadget.dylib \
		&& $$CODESIGN -f -s "$$IOS_CERTID" build/$*/lib/frida-gadget.dylib
	@touch $@

build/frida-macos-universal/lib/frida-gadget.dylib: build/.core-macos-stamp-frida-macos-x86 build/.core-macos-stamp-frida-macos-x86_64
	@mkdir -p $(@D)
	cp build/frida-macos-x86/lib/frida-gadget.dylib $(@D)/frida-gadget-x86.dylib
	cp build/frida-macos-x86_64/lib/frida-gadget.dylib $(@D)/frida-gadget-x86_64.dylib
	. build/frida-meson-env-macos-$(build_arch).rc \
		&& $$LIPO $(@D)/frida-gadget-x86.dylib $(@D)/frida-gadget-x86_64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	rm $(@D)/frida-gadget-*.dylib
	mv $@.tmp $@
build/frida-ios-universal/lib/frida-gadget.dylib: build/.core-ios-stamp-frida-ios-x86 build/.core-ios-stamp-frida-ios-x86_64 build/.core-ios-stamp-frida-ios-arm build/.core-ios-stamp-frida-ios-arm64
	@mkdir -p $(@D)
	cp build/frida-ios-x86/lib/frida-gadget.dylib $(@D)/frida-gadget-x86.dylib
	cp build/frida-ios-x86_64/lib/frida-gadget.dylib $(@D)/frida-gadget-x86_64.dylib
	cp build/frida-ios-arm/lib/frida-gadget.dylib $(@D)/frida-gadget-arm.dylib
	cp build/frida-ios-arm64/lib/frida-gadget.dylib $(@D)/frida-gadget-arm64.dylib
	. build/frida-env-ios-arm64.rc \
		&& $$LIPO $(@D)/frida-gadget-x86.dylib $(@D)/frida-gadget-x86_64.dylib $(@D)/frida-gadget-arm.dylib $(@D)/frida-gadget-arm64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	rm $(@D)/frida-gadget-*.dylib
	mv $@.tmp $@

check-core-macos: build/frida-macos-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for macOS
	build/tmp-macos-x86/frida-core/tests/frida-tests $(test_args)
	build/tmp-macos-x86_64/frida-core/tests/frida-tests $(test_args)
check-core-macos-thin: build/frida_thin-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for macOS without cross-arch support
	build/tmp_thin-macos-x86_64/frida-core/tests/frida-tests $(test_args)

gadget-macos: build/frida-macos-universal/lib/frida-gadget.dylib ##@gadget Build for macOS
gadget-macos-thin: core-macos-thin ##@gadget Build for macOS without cross-arch support
gadget-ios: build/frida-ios-universal/lib/frida-gadget.dylib ##@gadget Build for iOS
gadget-ios-thin: core-ios-thin ##@gadget Build for iOS without cross-arch support


python-macos: build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so ##@python Build Python bindings for macOS
python-macos-thin: build/tmp_thin-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp ##@python Build Python bindings for macOS without cross-arch support

define make-python-rule
build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp: build/.frida-python-submodule-stamp build/$1-%/lib/pkgconfig/frida-core-1.0.pc
	. build/$1-meson-env-macos-$$(build_arch).rc; \
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
build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-macos-x86/frida-$(PYTHON_NAME)/.frida-stamp build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	mkdir -p $(@D)
	cp build/frida-macos-x86/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-32.so
	cp build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-64.so
	. build/frida-env-macos-$(build_arch).rc \
		&& $$LIPO $(@D)/_frida-32.so $(@D)/_frida-64.so -create -output $@
	rm $(@D)/_frida-32.so $(@D)/_frida-64.so

check-python-macos: python-macos ##@python Test Python bindings for macOS
	export PYTHONPATH="$(shell pwd)/build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& $(PYTHON) -m unittest discover
check-python-macos-thin: python-macos-thin ##@python Test Python bindings for macOS without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-macos-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& $(PYTHON) -m unittest discover


node-macos: build/frida-macos-$(build_arch)/lib/node_modules/frida ##@node Build Node.js bindings for macOS
node-macos-thin: build/frida_thin-macos-x86_64/lib/node_modules/frida ##@node Build Node.js bindings for macOS without cross-arch support

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
		&& . ../build/$1-env-macos-$$(build_arch).rc && $$$$STRIP $$$$STRIP_FLAGS ../$$@.tmp/build/frida_binding.node \
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
check-node-macos: node-macos ##@node Test Node.js bindings for macOS
	$(call run-node-tests,frida-macos-$(build_arch),$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))
check-node-macos-thin: node-macos-thin ##@node Test Node.js bindings for macOS without cross-arch support
	$(call run-node-tests,frida_thin-macos-$(build_arch),$(FRIDA),$(NODE_BIN_DIR),$(NODE),$(NPM))


tools-macos: build/frida-macos-universal/bin/frida build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida_tools ##@tools Build CLI tools for macOS
tools-macos-thin: build/tmp_thin-macos-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp ##@tools Build CLI tools for macOS without cross-arch support

define make-tools-rule
build/$2-%/frida-tools-$$(PYTHON_NAME)/.frida-stamp: build/.frida-tools-submodule-stamp build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp
	. build/$1-meson-env-macos-$$(build_arch).rc; \
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
			$$$$cross_args \
			-Dpython=$$(PYTHON) \
			frida-tools $$$$builddir || exit 1; \
	fi; \
	$$(NINJA) -C $$$$builddir install || exit 1
	@touch $$@
endef
$(eval $(call make-tools-rule,frida,tmp))
$(eval $(call make-tools-rule,frida_thin,tmp_thin))
build/frida-macos-universal/bin/frida: build/tmp-macos-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp
	mkdir -p build/frida-macos-universal/bin
	for tool in $(frida_tools); do \
		cp build/frida-macos-x86_64/bin/$$tool build/frida-macos-universal/bin/; \
	done
build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida_tools: build/tmp-macos-x86_64/frida-tools-$(PYTHON_NAME)/.frida-stamp
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/frida_tools $@
	@touch $@

check-tools-macos: tools-macos ##@tools Test CLI tools for macOS
	export PYTHONPATH="$(shell pwd)/build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& $(PYTHON) -m unittest discover
check-tools-macos-thin: tools-macos-thin ##@tools Test CLI tools for macOS without cross-arch support
	export PYTHONPATH="$(shell pwd)/build/frida_thin-macos-x86_64/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& $(PYTHON) -m unittest discover


.PHONY: \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-update-submodule-stamp \
	gum-macos gum-macos-thin \
		gum-ios gum-ios-thin \
		gum-android-x86 gum-android-x86_64 \
		gum-android-arm gum-android-arm64 \
		check-gum-macos check-gum-macos-thin \
		frida-gum-update-submodule-stamp \
	core-macos core-macos-thin \
		core-ios core-ios-thin \
		core-android-x86 core-android-x86_64 \
		core-android-arm core-android-arm64 \
		check-core-macos check-core-macos-thin \
		frida-core-update-submodule-stamp \
	gadget-macos gadget-macos-thin \
		gadget-ios gadget-ios-thin \
	python-macos python-macos-thin \
		check-python-macos check-python-macos-thin \
		frida-python-update-submodule-stamp \
	node-macos node-macos-thin \
		check-node-macos check-node-macos-thin \
		frida-node-update-submodule-stamp \
	tools-macos tools-macos-thin \
		check-tools-macos check-tools-macos-thin \
		frida-tools-update-submodule-stamp \
	glib glib-symlinks \
	v8 v8-symlinks
.SECONDARY:
