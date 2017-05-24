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
	print "  \$$ make $${target_color}python-macos $${variable_color}PYTHON$${reset_color}=/usr/local/bin/python3.6\n"; \
	print "  \$$ make $${target_color}node-macos $${variable_color}NODE$${reset_color}=/usr/local/bin/node\n"; \
	print "\n";

help:
	@LC_ALL=C perl -e '$(HELP_FUN)' $(MAKEFILE_LIST)


include releng/common.mk

distclean: clean-submodules
	rm -rf build/

clean: clean-submodules
	rm -f build/*-clang*
	rm -f build/*-pkg-config
	rm -f build/*-stamp
	rm -f build/*-strip
	rm -f build/*.rc
	rm -f build/*.sh
	rm -f build/*.site
	rm -f build/*.txt
	rm -f build/frida-version.h
	rm -rf build/frida-macos-x86
	rm -rf build/frida-macos-x86_64
	rm -rf build/frida-macos-universal
	rm -rf build/frida-ios-universal
	rm -rf build/frida-ios-x86
	rm -rf build/frida-ios-x86_64
	rm -rf build/frida-ios-arm
	rm -rf build/frida-ios-arm64
	rm -rf build/frida-android-x86
	rm -rf build/frida-android-arm
	rm -rf build/frida-android-arm64
	rm -rf build/tmp-macos-x86
	rm -rf build/tmp-macos-x86_64
	rm -rf build/tmp-macos-universal
	rm -rf build/tmp-ios-x86
	rm -rf build/tmp-ios-x86_64
	rm -rf build/tmp-ios-arm
	rm -rf build/tmp-ios-arm64
	rm -rf build/tmp-ios-universal
	rm -rf build/tmp-android-x86
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-android-arm64

clean-submodules:
	cd capstone && git clean -xfd
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd


build/frida-%/lib/pkgconfig/capstone.pc: build/frida-env-%.rc build/.capstone-submodule-stamp
	. build/frida-env-$*.rc \
		&& export PACKAGE_TARNAME=capstone \
		&& . $$CONFIG_SITE \
		&& case $* in \
			*-x86)    capstone_archs="x86"         ;; \
			*-x86_64) capstone_archs="x86"         ;; \
			*-arm)    capstone_archs="arm"         ;; \
			*-arm64)  capstone_archs="aarch64 arm" ;; \
		esac \
		&& make -C capstone \
			PREFIX=$$frida_prefix \
			BUILDDIR=../build/tmp-$*/capstone \
			CAPSTONE_ARCHS="$$capstone_archs" \
			CAPSTONE_SHARED=$$enable_shared \
			CAPSTONE_STATIC=$$enable_static \
			install


gum-macos: build/frida-macos-x86/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for macOS
gum-ios: build/frida-ios-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for iOS
gum-android: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android

build/.frida-gum-npm-stamp: build/frida-env-macos-$(build_arch).rc
	@$(NPM) --version &>/dev/null || (echo "\033[31mOops. It appears Node.js is not installed.\nWe need it for processing JavaScript code at build-time.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	. build/frida-env-macos-$(build_arch).rc && cd frida-gum/bindings/gumjs && npm install
	@touch $@

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/.frida-gum-submodule-stamp build/.frida-gum-npm-stamp build/frida-%/lib/pkgconfig/capstone.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=build/tmp-$*/frida-gum; \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-$* \
			--cross-file build/frida-$*.txt \
			$(frida_gum_flags) \
			frida-gum $$builddir || exit 1; \
	fi; \
	$(NINJA) -C $$builddir install || exit 1
	@touch -c $@

check-gum-macos: build/frida-macos-x86/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for macOS
	build/tmp-macos-x86/frida-gum/tests/gum-tests $(test_args)
	build/tmp-macos-x86_64/frida-gum/tests/gum-tests $(test_args)


core-macos: build/frida-macos-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for macOS
core-ios: build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for iOS
core-android: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android

build/tmp-macos-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-macos-%/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-macos-$* \
			--cross-file build/frida-macos-$*.txt \
			$(frida_core_flags) \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-macos-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-macos-x86/frida-core/lib/agent/frida-agent.dylib \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-macos-x86_64/frida-core/lib/agent/frida-agent.dylib \
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
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-ios-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-ios-x86/frida-core/lib/agent/frida-agent.dylib \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-ios-x86_64/frida-core/lib/agent/frida-agent.dylib \
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
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-ios-arm64/frida-core/src/frida-helper \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-ios-arm/frida-core/lib/agent/frida-agent.dylib \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-ios-arm64/frida-core/lib/agent/frida-agent.dylib \
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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-android-x86/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-android-x86_64/frida-core/src/frida-helper \
			-Dwith-32bit-loader=$(FRIDA)/build/tmp-android-x86/frida-core/lib/loader/frida-loader.so \
			-Dwith-64bit-loader=$(FRIDA)/build/tmp-android-x86_64/frida-core/lib/loader/frida-loader.so \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-android-x86/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-android-x86_64/frida-core/lib/agent/frida-agent.so \
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
			-Dwith-32bit-helper=$(FRIDA)/build/tmp-android-arm/frida-core/src/frida-helper \
			-Dwith-64bit-helper=$(FRIDA)/build/tmp-android-arm64/frida-core/src/frida-helper \
			-Dwith-32bit-loader=$(FRIDA)/build/tmp-android-arm/frida-core/lib/loader/frida-loader.so \
			-Dwith-64bit-loader=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/loader/frida-loader.so \
			-Dwith-32bit-agent=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so \
			-Dwith-64bit-agent=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@

build/frida-macos-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp-macos-x86/frida-core/.frida-agent-stamp build/tmp-macos-x86_64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-macos-$*/frida-core install
	@touch $@
build/frida-ios-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-x86/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-x86/frida-core install
	@touch $@
build/frida-ios-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-x86/frida-core/.frida-agent-stamp build/tmp-ios-x86_64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-x86_64/frida-core install
	@touch $@
build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-arm/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-arm/frida-core install
	@touch $@
build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-arm/frida-core/.frida-agent-stamp build/tmp-ios-arm64/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-ios-arm64/frida-core install
	@touch $@
build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86/frida-core install
	@touch $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-loader-and-agent-stamp build/tmp-android-x86_64/frida-core/.frida-helper-loader-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-x86_64/frida-core install
	@touch $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm/frida-core install
	@touch $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-loader-and-agent-stamp build/tmp-android-arm64/frida-core/.frida-helper-loader-and-agent-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-arm64/frida-core install
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
build/tmp-android-%/frida-core/.frida-helper-loader-and-agent-stamp: build/tmp-android-%/frida-core/.frida-ninja-stamp
	. build/frida-meson-env-macos-$(build_arch).rc && $(NINJA) -C build/tmp-android-$*/frida-core src/frida-helper lib/loader/frida-loader.so lib/agent/frida-agent.so
	@touch $@

build/frida-macos-universal/lib/FridaGadget.dylib: build/frida-macos-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#macos-and-macos"; exit 1; fi
	mkdir -p $(@D)
	cp build/frida-macos-x86/lib/FridaGadget.dylib $(@D)/FridaGadget-x86.dylib
	cp build/frida-macos-x86_64/lib/FridaGadget.dylib $(@D)/FridaGadget-x86_64.dylib
	. build/frida-env-macos-x86_64.rc \
		&& $$STRIP $$STRIP_FLAGS $(@D)/FridaGadget-x86.dylib $(@D)/FridaGadget-x86_64.dylib \
		&& $$LIPO $(@D)/FridaGadget-x86.dylib $(@D)/FridaGadget-x86_64.dylib -create -output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/../Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	rm $(@D)/FridaGadget-*.dylib
	mv $@.tmp $@
build/frida-ios-universal/lib/FridaGadget.dylib: build/frida-ios-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-x86_64/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#macos-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/frida-ios-x86/lib/FridaGadget.dylib $(@D)/FridaGadget-x86.dylib
	cp build/frida-ios-x86_64/lib/FridaGadget.dylib $(@D)/FridaGadget-x86_64.dylib
	cp build/frida-ios-arm/lib/FridaGadget.dylib $(@D)/FridaGadget-arm.dylib
	cp build/frida-ios-arm64/lib/FridaGadget.dylib $(@D)/FridaGadget-arm64.dylib
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP $$STRIP_FLAGS $(@D)/FridaGadget-x86.dylib $(@D)/FridaGadget-x86_64.dylib $(@D)/FridaGadget-arm.dylib $(@D)/FridaGadget-arm64.dylib \
		&& $$LIPO $(@D)/FridaGadget-x86.dylib $(@D)/FridaGadget-x86_64.dylib $(@D)/FridaGadget-arm.dylib $(@D)/FridaGadget-arm64.dylib -create -output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	rm $(@D)/FridaGadget-*.dylib
	mv $@.tmp $@

check-core-macos: build/frida-macos-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for macOS
	build/tmp-macos-x86/frida-core/tests/frida-tests $(test_args)
	build/tmp-macos-x86_64/frida-core/tests/frida-tests $(test_args)

server-macos: build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for macOS
server-ios: build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for iOS
server-android: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@server Build for Android

gadget-macos: build/frida-macos-universal/lib/FridaGadget.dylib ##@gadget Build for macOS
gadget-ios: build/frida-ios-universal/lib/FridaGadget.dylib ##@gadget Build for iOS
gadget-android: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@gadget Build for Android

python-macos: build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-macos-universal/bin/frida ##@python Build Python bindings for macOS

build/tmp-%/frida-$(PYTHON_NAME)/.frida-stamp: build/.frida-python-submodule-stamp build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	. build/frida-meson-env-macos-$(build_arch).rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		mkdir -p $$builddir; \
		$(MESON) \
			--prefix $(FRIDA)/build/frida-$* \
			--cross-file build/frida-$*.txt \
			-Dwith-python=$(PYTHON) \
			frida-python $$builddir || exit 1; \
	fi; \
	$(NINJA) -C $$builddir install || exit 1
	@touch $@

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
		&& $$STRIP $$STRIP_FLAGS $(@D)/_frida-32.so $(@D)/_frida-64.so \
		&& $$LIPO $(@D)/_frida-32.so $(@D)/_frida-64.so -create -output $@
	rm $(@D)/_frida-32.so $(@D)/_frida-64.so

build/frida-macos-universal/bin/frida: build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	mkdir -p build/frida-macos-universal/bin
	for tool in $(frida_python_tools); do \
		cp build/frida-macos-x86_64/bin/$$tool build/frida-macos-universal/bin/; \
	done

check-python-macos: python-macos ##@python Test Python bindings for macOS
	export PYTHONPATH="$(shell pwd)/build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& if $(PYTHON) -c "import sys; v = sys.version_info; can_execute_modules = v[0] > 2 or (v[0] == 2 and v[1] >= 7); sys.exit(0 if can_execute_modules else 1)"; then \
			$(PYTHON) -m unittest discover; \
		else \
			unit2 discover; \
		fi

install-python-macos: python-macos ##@python Install Python bindings for macOS
	@awk '/install_requires=\[/,/\],/' frida-python/src/setup.py | sed -n 's/.*"\(.*\)".*/\1/p' | $(PYTHON) -mpip install -r /dev/stdin
	sitepackages=`$(PYTHON) -c 'import site; print(site.getsitepackages()[0])'` \
		&& cp -r "build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/" "$$sitepackages"

uninstall-python-macos: ##@python Uninstall Python bindings for macos
	cd `$(PYTHON) -c 'import site; print(site.getsitepackages()[0])'` \
		&& rm -rf _frida.so frida


node-macos: build/frida-macos-$(build_arch)/lib/node_modules/frida build/.frida-node-submodule-stamp ##@node Build Node.js bindings for macOS

build/frida-%/lib/node_modules/frida: build/frida-%/lib/pkgconfig/frida-core-1.0.pc build/.frida-node-submodule-stamp
	@$(NPM) --version &>/dev/null || (echo "\033[31mOops. It appears Node.js is not installed.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
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
		&& . ../build/frida-env-macos-$(build_arch).rc && $$STRIP $$STRIP_FLAGS ../$@.tmp/build/frida_binding.node \
		&& mv ../$@.tmp ../$@

check-node-macos: build/frida-macos-$(build_arch)/lib/node_modules/frida ##@node Test Node.js bindings for macOS
	cd $< && $(NODE) --expose-gc node_modules/mocha/bin/_mocha --timeout 60000


install-macos: install-python-macos ##@utilities Install frida utilities (frida{-discover,-kill,-ls-devices,-ps,-trace})
	for tool in $(frida_python_tools); do \
		b="build/frida-macos-universal/bin/$$tool"; \
		p="$(PREFIX)/bin/$$tool"; \
		t="$$(mktemp -t frida)"; \
		grep -v 'sys.path.insert' "$$b" > "$$t"; \
		chmod +x "$$t"; \
		if [ -w "$(PREFIX)/bin" ]; then \
			mv "$$t" "$$p"; \
		else \
			sudo mv "$$t" "$$p"; \
		fi \
	done

uninstall-macos: uninstall-python-macos ##@utilities Uninstall frida utilities
	@for tool in $(frida_python_tools); do \
		if which "$$tool" &> /dev/null; then \
			p=`which "$$tool"`; \
			if [ -w "$$(dirname "$$p")" ]; then \
				rm -f "$$p"; \
			else \
				sudo rm -f "$$p"; \
			fi \
		fi \
	done


.PHONY: \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	capstone-update-submodule-stamp \
	gum-macos gum-ios gum-android check-gum-macos frida-gum-update-submodule-stamp \
	core-macos core-ios core-android check-core-macos check-core-android-arm64 frida-core-update-submodule-stamp \
	server-macos server-ios server-android \
	gadget-macos gadget-ios gadget-android \
	python-macos check-python-macos install-python-macos uninstall-python-macos frida-python-update-submodule-stamp \
	node-macos check-node-macos frida-node-update-submodule-stamp \
	install-macos uninstall-macos \
	glib glib-shell glib-symlinks
.SECONDARY:
