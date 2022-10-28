include config.mk

build_arch := $(shell releng/detect-arch.sh)
ifeq ($(build_arch), arm64)
build_cpu_flavor := apple_silicon
else
build_cpu_flavor := intel
endif
ios_arm64eoabi_target := $(shell test -d /Applications/Xcode-11.7.app && echo build/frida-ios-arm64eoabi/usr/lib/pkgconfig/frida-core-1.0.pc)
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


include releng/frida.mk

distclean: clean-submodules
	rm -rf build/
	rm -rf deps/

clean: clean-submodules
	rm -f build/*-clang*
	rm -f build/*-pkg-config
	rm -f build/*-stamp
	rm -f build/*-strip
	rm -f build/*.deb
	rm -f build/*.rc
	rm -f build/*.sh
	rm -f build/*.site
	rm -f build/*.tar.bz2
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
	cd frida-gum && git clean -xfd
	cd frida-core && git clean -xfd
	cd frida-python && git clean -xfd
	cd frida-node && git clean -xfd
	cd frida-tools && git clean -xfd


gum-macos: gum-macos-$(build_cpu_flavor) ##@gum Build for macOS
gum-macos-apple_silicon: build/frida-macos-arm64/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-arm64e/lib/pkgconfig/frida-gum-1.0.pc
gum-macos-intel: build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc
gum-ios: build/frida-ios-arm64/usr/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for iOS
gum-android-x86: build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86
gum-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/x86-64
gum-android-arm: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/arm
gum-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android/arm64

define make-gum-rules
build/$1-%/lib/pkgconfig/frida-gum-1.0.pc: build/$1-env-%.rc build/.frida-gum-submodule-stamp
	. build/$1-env-$$*.rc; \
	builddir=build/$2-$$*/frida-gum; \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		$$(call meson-setup-for-env,$1,$$*) \
			--prefix $$(FRIDA)/build/$1-$$* \
			$$(frida_gum_flags) \
			frida-gum $$$$builddir || exit 1; \
	fi; \
	$$(MESON) install -C $$$$builddir || exit 1
	@touch -c $$@
endef
$(eval $(call make-gum-rules,frida,tmp))
$(eval $(call make-gum-rules,frida_thin,tmp_thin))

ifeq ($(build_arch), arm64)
check-gum-macos: build/frida-macos-arm64/lib/pkgconfig/frida-gum-1.0.pc build/frida-macos-arm64e/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for macOS
	build/tmp-macos-arm64/frida-gum/tests/gum-tests $(test_args)
	runner=build/tmp-macos-arm64e/frida-gum/tests/gum-tests; \
	if $$runner --help &>/dev/null; then \
		$$runner $(test_args); \
	fi
else
check-gum-macos: build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	build/tmp-macos-x86_64/frida-gum/tests/gum-tests $(test_args)
endif


core-macos: core-macos-$(build_cpu_flavor) ##@core Build for macOS
core-macos-apple_silicon: build/frida-macos-arm64/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-arm64e/lib/pkgconfig/frida-core-1.0.pc
core-macos-intel: build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc
core-ios: build/frida-ios-universal/usr/bin/frida-server ##@core Build for iOS
core-android-x86: build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86
core-android-x86_64: build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/x86-64
core-android-arm: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/arm
core-android-arm64: build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android/arm64

build/tmp-macos-arm64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-macos-arm64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-macos-arm64.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,macos-arm64) \
			--prefix $(FRIDA)/build/frida-macos-arm64 \
			$(frida_core_flags) \
			-Dhelper_modern=$(FRIDA)/build/tmp-macos-arm64e/frida-core/src/frida-helper \
			-Dhelper_legacy=$(FRIDA)/build/tmp-macos-arm64/frida-core/src/frida-helper \
			-Dagent_modern=$(FRIDA)/build/tmp-macos-arm64e/frida-core/lib/agent/frida-agent.dylib \
			-Dagent_legacy=$(FRIDA)/build/tmp-macos-arm64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-macos-arm64e/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-macos-arm64e/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-macos-arm64e.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,macos-arm64e) \
			--prefix $(FRIDA)/build/frida-macos-arm64e \
			$(frida_core_flags) \
			-Dhelper_modern=$(FRIDA)/build/tmp-macos-arm64e/frida-core/src/frida-helper \
			-Dhelper_legacy=$(FRIDA)/build/tmp-macos-arm64/frida-core/src/frida-helper \
			-Dagent_modern=$(FRIDA)/build/tmp-macos-arm64e/frida-core/lib/agent/frida-agent.dylib \
			-Dagent_legacy=$(FRIDA)/build/tmp-macos-arm64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-macos-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-macos-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-macos-x86_64.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,macos-x86_64) \
			--prefix $(FRIDA)/build/frida-macos-x86_64 \
			$(frida_core_flags) \
			-Dhelper_modern=$(FRIDA)/build/tmp-macos-x86_64/frida-core/src/frida-helper \
			-Dagent_modern=$(FRIDA)/build/tmp-macos-x86_64/frida-core/lib/agent/frida-agent.dylib \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86/lib/pkgconfig/frida-gum-1.0.pc
	if [ "$(FRIDA_AGENT_EMULATED)" == "yes" ]; then \
		agent_emulated_legacy=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so; \
	fi; \
	. build/frida-env-android-x86.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,android-x86) \
			--prefix $(FRIDA)/build/frida-android-x86 \
			$(frida_core_flags) \
			-Dagent_emulated_legacy=$$agent_emulated_legacy \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-x86_64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-x86_64/lib/pkgconfig/frida-gum-1.0.pc
	if [ "$(FRIDA_AGENT_EMULATED)" == "yes" ]; then \
		agent_emulated_modern=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/agent/frida-agent.so; \
		agent_emulated_legacy=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so; \
	fi; \
	. build/frida-env-android-x86_64.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,android-x86_64) \
			--prefix $(FRIDA)/build/frida-android-x86_64 \
			$(frida_core_flags) \
			-Dhelper_modern=$(FRIDA)/build/tmp-android-x86_64/frida-core/src/frida-helper \
			-Dhelper_legacy=$(FRIDA)/build/tmp-android-x86/frida-core/src/frida-helper \
			-Dagent_modern=$(FRIDA)/build/tmp-android-x86_64/frida-core/lib/agent/frida-agent.so \
			-Dagent_legacy=$(FRIDA)/build/tmp-android-x86/frida-core/lib/agent/frida-agent.so \
			-Dagent_emulated_modern=$$agent_emulated_modern \
			-Dagent_emulated_legacy=$$agent_emulated_legacy \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-arm/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-android-arm.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,android-arm) \
			--prefix $(FRIDA)/build/frida-android-arm \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp-android-arm64/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-android-arm64.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,android-arm64) \
			--prefix $(FRIDA)/build/frida-android-arm64 \
			$(frida_core_flags) \
			-Dhelper_modern=$(FRIDA)/build/tmp-android-arm64/frida-core/src/frida-helper \
			-Dhelper_legacy=$(FRIDA)/build/tmp-android-arm/frida-core/src/frida-helper \
			-Dagent_modern=$(FRIDA)/build/tmp-android-arm64/frida-core/lib/agent/frida-agent.so \
			-Dagent_legacy=$(FRIDA)/build/tmp-android-arm/frida-core/lib/agent/frida-agent.so \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@
build/tmp_thin-%/frida-core/.frida-ninja-stamp: build/.frida-core-submodule-stamp build/frida_thin-%/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida_thin-env-$*.rc; \
	builddir=$(@D); \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup-thin,$*) \
			--prefix $(FRIDA)/build/frida_thin-$* \
			$(frida_core_flags) \
			frida-core $$builddir || exit 1; \
	fi
	@touch $@

ifeq ($(FRIDA_AGENT_EMULATED), yes)
legacy_agent_emulated_dep := build/tmp-android-arm/frida-core/.frida-agent-stamp
modern_agent_emulated_dep := build/tmp-android-arm64/frida-core/.frida-agent-stamp
endif

build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-macos-x86_64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-macos-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-macos-x86_64.rc && $(MESON) install -C build/tmp-macos-x86_64/frida-core
	@touch $@
build/frida-macos-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-macos-arm64/frida-core/.frida-helper-and-agent-stamp build/tmp-macos-arm64e/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-macos-arm64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-macos-arm64.rc && $(MESON) install -C build/tmp-macos-arm64/frida-core
	@touch $@
build/frida-macos-arm64e/lib/pkgconfig/frida-core-1.0.pc: build/tmp-macos-arm64/frida-core/.frida-helper-and-agent-stamp build/tmp-macos-arm64e/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-macos-arm64e/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-macos-arm64e.rc && $(MESON) install -C build/tmp-macos-arm64e/frida-core
	@touch $@
build/frida-android-x86/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp $(legacy_agent_emulated_dep)
	@rm -f build/tmp-android-x86/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-android-x86.rc && $(MESON) install -C build/tmp-android-x86/frida-core
	@touch $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-x86/frida-core/.frida-helper-and-agent-stamp build/tmp-android-x86_64/frida-core/.frida-helper-and-agent-stamp $(legacy_agent_emulated_dep) $(modern_agent_emulated_dep)
	@rm -f build/tmp-android-x86_64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-android-x86_64.rc && $(MESON) install -C build/tmp-android-x86_64/frida-core
	@touch $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-android-arm.rc && $(MESON) install -C build/tmp-android-arm/frida-core
	@touch $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-android-arm/frida-core/.frida-helper-and-agent-stamp build/tmp-android-arm64/frida-core/.frida-helper-and-agent-stamp
	@rm -f build/tmp-android-arm64/frida-core/src/frida-data-{helper,agent}*
	. build/frida-env-android-arm64.rc && $(MESON) install -C build/tmp-android-arm64/frida-core
	@touch $@
build/frida_thin-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_thin-%/frida-core/.frida-ninja-stamp
	. build/frida_thin-env-$*.rc && $(MESON) install -C build/tmp_thin-$*/frida-core
	@touch $@

build/tmp-macos-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-macos-%/frida-core/.frida-ninja-stamp
	. build/frida-env-macos-$*.rc && ninja -C build/tmp-macos-$*/frida-core src/frida-helper lib/agent/frida-agent.dylib
	@touch $@
build/tmp-macos-%/frida-core/.frida-agent-stamp: build/tmp-macos-%/frida-core/.frida-ninja-stamp
	. build/frida-env-macos-$*.rc && ninja -C build/tmp-macos-$*/frida-core lib/agent/frida-agent.dylib
	@touch $@
build/tmp-android-%/frida-core/.frida-helper-and-agent-stamp: build/tmp-android-%/frida-core/.frida-ninja-stamp
	. build/frida-env-android-$*.rc && ninja -C build/tmp-android-$*/frida-core src/frida-helper lib/agent/frida-agent.so
	@touch $@
build/tmp-android-%/frida-core/.frida-agent-stamp: build/tmp-android-%/frida-core/.frida-ninja-stamp
	. build/frida-env-android-$*.rc && ninja -C build/tmp-android-$*/frida-core lib/agent/frida-agent.so
	@touch $@

build/frida-macos-universal/lib/frida/frida-gadget.dylib: \
		build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc \
		build/frida-macos-arm64/lib/pkgconfig/frida-core-1.0.pc \
		build/frida-macos-arm64e/lib/pkgconfig/frida-core-1.0.pc
	@mkdir -p $(@D)
	. build/frida-env-macos-$(build_arch).rc \
		&& $$LIPO \
			build/frida-macos-x86_64/lib/frida/frida-gadget.dylib \
			build/frida-macos-arm64/lib/frida/frida-gadget.dylib \
			build/frida-macos-arm64e/lib/frida/frida-gadget.dylib \
			-create \
			-output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/../Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$MACOS_CERTID" $@.tmp \
		&& mv $@.tmp $@

build/frida-ios-universal/usr/bin/frida-server: \
		build/frida-ios-arm64/usr/lib/pkgconfig/frida-core-1.0.pc \
		build/frida-ios-arm64e/usr/lib/pkgconfig/frida-core-1.0.pc \
		$(ios_arm64eoabi_target)
	@mkdir -p $(@D) build/frida-ios-universal/usr/lib/frida
	. build/frida-env-ios-arm64e.rc \
		&& agent=build/frida-ios-universal/usr/lib/frida/frida-agent.dylib \
		&& $$LIPO \
			build/frida-ios-arm64/usr/lib/frida/frida-agent.dylib \
			build/frida-ios-arm64e/usr/lib/frida/frida-agent.dylib \
			-create \
			-output $$agent \
		&& $$INSTALL_NAME_TOOL -id FridaAgent $$agent \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $$agent \
		&& slices=() \
		&& for arch in arm64 arm64eoabi arm64e; do \
			if [ -f build/frida-ios-$$arch/usr/bin/frida-server ]; then \
				cp build/frida-ios-$$arch/usr/bin/frida-server $@-$$arch || exit 1; \
				$$CODESIGN -f -s "$$IOS_CERTID" --entitlements frida-core/server/frida-server.xcent $@-$$arch || exit 1; \
				slices+=($@-$$arch); \
			fi \
		done \
		&& ./releng/mkfatmacho.py $@.tmp "$${slices[@]}" \
		&& rm $@-* \
		&& mv $@.tmp $@
build/frida-ios-universal/usr/lib/frida/frida-gadget.dylib: \
		build/frida-ios-x86_64/usr/lib/pkgconfig/frida-core-1.0.pc \
		build/frida-ios-arm64/usr/lib/pkgconfig/frida-core-1.0.pc \
		build/frida-ios-arm64e/usr/lib/pkgconfig/frida-core-1.0.pc
	@mkdir -p $(@D)
	. build/frida-env-ios-arm64e.rc \
		&& $$LIPO \
			build/frida-ios-x86_64/usr/lib/frida/frida-gadget.dylib \
			build/frida-ios-arm64/usr/lib/frida/frida-gadget.dylib \
			build/frida-ios-arm64e/usr/lib/frida/frida-gadget.dylib \
			-create \
			-output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp \
		&& mv $@.tmp $@

define make-ios-env-rule
build/frida-env-ios-$1.rc: releng/setup-env.sh build/frida-version.h
	@if [ $1 != $$(build_os_arch) ]; then \
		cross=yes; \
	else \
		cross=no; \
	fi; \
	for os_arch in $$(build_os_arch) ios-$1; do \
		if [ ! -f build/frida-env-$$$$os_arch.rc ]; then \
			FRIDA_HOST=$$$$os_arch \
			FRIDA_CROSS=$$$$cross \
			FRIDA_PREFIX="$$(abspath build/frida-ios-$1/usr)" \
			FRIDA_ASAN=$$(FRIDA_ASAN) \
			XCODE11="$$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
endef

$(eval $(call make-ios-env-rule,x86_64))
$(eval $(call make-ios-env-rule,arm64))
$(eval $(call make-ios-env-rule,arm64e))
$(eval $(call make-ios-env-rule,arm64eoabi))

build/frida-ios-%/usr/lib/pkgconfig/frida-gum-1.0.pc: build/frida-env-ios-%.rc build/.frida-gum-submodule-stamp
	. build/frida-env-ios-$*.rc; \
	builddir=build/tmp-ios-$*/frida-gum; \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,ios-$*) \
			--prefix /usr \
			$(frida_gum_flags) \
			frida-gum $$builddir || exit 1; \
	fi \
		&& $(MESON) compile -C $$builddir \
		&& DESTDIR="$(abspath build/frida-ios-$*)" $(MESON) install -C $$builddir
	@touch $@
build/frida-ios-%/usr/lib/pkgconfig/frida-core-1.0.pc: build/.frida-core-submodule-stamp build/frida-ios-%/usr/lib/pkgconfig/frida-gum-1.0.pc
	. build/frida-env-ios-$*.rc; \
	builddir=build/tmp-ios-$*/frida-core; \
	if [ ! -f $$builddir/build.ninja ]; then \
		$(call meson-setup,ios-$*) \
			--prefix /usr \
			$(frida_core_flags) \
			-Dassets=installed \
			frida-core $$builddir || exit 1; \
	fi \
		&& $(MESON) compile -C $$builddir \
		&& DESTDIR="$(abspath build/frida-ios-$*)" $(MESON) install -C $$builddir
	@touch $@

ifeq ($(build_arch), arm64)
check-core-macos: build/frida-macos-arm64/lib/pkgconfig/frida-core-1.0.pc build/frida-macos-arm64e/lib/pkgconfig/frida-core-1.0.pc ##@core Run tests for macOS
	build/tmp-macos-arm64/frida-core/tests/frida-tests $(test_args)
	runner=build/tmp-macos-arm64e/frida-core/tests/frida-tests; \
	if $$runner --help &>/dev/null; then \
		$$runner $(test_args); \
	fi
else
check-core-macos: build/frida-macos-x86_64/lib/pkgconfig/frida-core-1.0.pc
	build/tmp-macos-x86_64/frida-core/tests/frida-tests $(test_args)
endif

gadget-macos: build/frida-macos-universal/lib/frida/frida-gadget.dylib ##@gadget Build for macOS
gadget-ios: build/frida-ios-universal/usr/lib/frida/frida-gadget.dylib ##@gadget Build for iOS

deb-ios: build/frida-ios-universal/usr/bin/frida-server
	export FRIDA_VERSION=$$(grep 'FRIDA_VERSION "' build/frida-version.h | awk '{ print $$3; }' | cut -f2 -d'"'); \
	frida-core/tools/package-server-ios.sh build/frida-ios-universal build/frida_$${FRIDA_VERSION}_iphoneos-arm.deb


python-macos: python-macos-$(build_cpu_flavor) ##@python Build Python bindings for macOS
python-macos-universal: build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so
python-macos-apple_silicon: build/frida-macos-apple_silicon/lib/$(PYTHON_NAME)/site-packages/frida build/frida-macos-apple_silicon/lib/$(PYTHON_NAME)/site-packages/_frida.so
python-macos-intel: build/frida-macos-intel/lib/$(PYTHON_NAME)/site-packages/frida build/frida-macos-intel/lib/$(PYTHON_NAME)/site-packages/_frida.so

define make-python-rule
build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp: build/.frida-python-submodule-stamp build/$1-%$(PYTHON_PREFIX)/lib/pkgconfig/frida-core-1.0.pc
	. build/$1-env-$$*.rc; \
	builddir=$$(@D); \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		$$(call meson-setup-for-env,$1,$$*) \
			--prefix $$(FRIDA)/build/$1-$$*$(PYTHON_PREFIX) \
			-Dpython=$$(PYTHON) \
			-Dpython_incdir=$$(PYTHON_INCDIR) \
			frida-python $$$$builddir || exit 1; \
	fi; \
	$$(MESON) install -C $$$$builddir || exit 1; \
	$$$$STRIP $$$$STRIP_FLAGS build/$1-$$*$(PYTHON_PREFIX)/lib/$$(PYTHON_NAME)/site-packages/_frida.so
	@touch $$@
endef
$(eval $(call make-python-rule,frida,tmp))
$(eval $(call make-python-rule,frida_thin,tmp_thin))
build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-macos-arm64e/frida-$(PYTHON_NAME)/.frida-stamp
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-arm64e/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida-macos-apple_silicon/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-macos-arm64e/frida-$(PYTHON_NAME)/.frida-stamp
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-arm64e/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida-macos-intel/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida-macos-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-macos-arm64/frida-$(PYTHON_NAME)/.frida-stamp build/tmp-macos-arm64e/frida-$(PYTHON_NAME)/.frida-stamp build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	mkdir -p $(@D)
	cp build/frida-macos-arm64/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-arm64.so
	cp build/frida-macos-arm64e/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-arm64e.so
	cp build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-x86_64.so
	. build/frida-env-macos-$(build_arch).rc \
		&& $$LIPO $(@D)/_frida-arm64.so $(@D)/_frida-arm64e.so $(@D)/_frida-x86_64.so -create -output $@
	rm $(@D)/_frida-arm64.so $(@D)/_frida-arm64e.so $(@D)/_frida-x86_64.so
build/frida-macos-apple_silicon/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-macos-arm64/frida-$(PYTHON_NAME)/.frida-stamp build/tmp-macos-arm64e/frida-$(PYTHON_NAME)/.frida-stamp
	mkdir -p $(@D)
	cp build/frida-macos-arm64/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-arm64.so
	cp build/frida-macos-arm64e/lib/$(PYTHON_NAME)/site-packages/_frida.so $(@D)/_frida-arm64e.so
	. build/frida-env-macos-$(build_arch).rc \
		&& $$LIPO $(@D)/_frida-arm64.so $(@D)/_frida-arm64e.so -create -output $@
	rm $(@D)/_frida-arm64.so $(@D)/_frida-arm64e.so
build/frida-macos-intel/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-macos-x86_64/frida-$(PYTHON_NAME)/.frida-stamp
	mkdir -p $(@D)
	cp build/frida-macos-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so $@
	@touch $@

define run-python-tests
	export PYTHONPATH="$(shell pwd)/build/frida-macos-$1/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& $2 $(PYTHON) -m unittest discover
endef
check-python-macos: check-python-macos-$(build_cpu_flavor) ##@python Test Python bindings for macOS
check-python-macos-apple_silicon: python-macos-apple_silicon
	$(call run-python-tests,apple_silicon,)
check-python-macos-intel: python-macos-intel
	$(call run-python-tests,intel,arch -x86_64)


node-macos: build/frida-macos-$(build_arch)/lib/node_modules/frida ##@node Build Node.js bindings for macOS

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


tools-macos: build/frida-macos-$(build_cpu_flavor)/bin/frida build/frida-macos-$(build_cpu_flavor)/lib/$(PYTHON_NAME)/site-packages/frida_tools ##@tools Build CLI tools for macOS

define make-tools-rule
build/$2-%/frida-tools-$$(PYTHON_NAME)/.frida-stamp: build/.frida-tools-submodule-stamp build/$2-%/frida-$$(PYTHON_NAME)/.frida-stamp
	. build/$1-env-$$*.rc; \
	builddir=$$(@D); \
	if [ ! -f $$$$builddir/build.ninja ]; then \
		$$(call meson-setup-for-env,$1,$$*) \
			--prefix $$(FRIDA)/build/$1-$$* \
			-Dpython=$$(PYTHON) \
			frida-tools $$$$builddir || exit 1; \
	fi; \
	$$(MESON) install -C $$$$builddir || exit 1
	@touch $$@
endef
$(eval $(call make-tools-rule,frida,tmp))
$(eval $(call make-tools-rule,frida_thin,tmp_thin))
build/frida-macos-$(build_cpu_flavor)/bin/frida: build/tmp-macos-$(build_arch)/frida-tools-$(PYTHON_NAME)/.frida-stamp
	mkdir -p build/frida-macos-$(build_cpu_flavor)/bin
	for tool in $(frida_tools); do \
		cp build/frida-macos-$(build_arch)/bin/$$tool build/frida-macos-$(build_cpu_flavor)/bin/; \
	done
build/frida-macos-$(build_cpu_flavor)/lib/$(PYTHON_NAME)/site-packages/frida_tools: \
		build/tmp-macos-$(build_arch)/frida-tools-$(PYTHON_NAME)/.frida-stamp \
		build/frida-macos-$(build_cpu_flavor)/lib/$(PYTHON_NAME)/site-packages/frida \
		build/frida-macos-$(build_cpu_flavor)/lib/$(PYTHON_NAME)/site-packages/_frida.so
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-macos-$(build_arch)/lib/$(PYTHON_NAME)/site-packages/frida_tools $@
	@touch $@

check-tools-macos: tools-macos ##@tools Test CLI tools for macOS
	export PYTHONPATH="$(shell pwd)/build/frida-macos-$(build_cpu_flavor)/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-tools \
		&& $(PYTHON) -m unittest discover


.PHONY: \
	distclean clean clean-submodules git-submodules git-submodule-stamps \
	gum-macos \
		gum-macos-apple_silicon gum-macos-intel \
		gum-ios \
		gum-android-x86 gum-android-x86_64 \
		gum-android-arm gum-android-arm64 \
		check-gum-macos \
		frida-gum-update-submodule-stamp \
	core-macos \
		core-macos-apple_silicon core-macos-intel \
		core-ios \
		core-android-x86 core-android-x86_64 \
		core-android-arm core-android-arm64 \
		check-core-macos \
		frida-core-update-submodule-stamp \
	gadget-macos \
		gadget-ios \
	deb-ios \
	python-macos \
		python-macos-universal python-macos-apple_silicon python-macos-intel \
		check-python-macos \
		check-python-macos-apple_silicon check-python-macos-intel \
		frida-python-update-submodule-stamp \
	node-macos \
		check-node-macos \
		frida-node-update-submodule-stamp \
	tools-macos \
		check-tools-macos \
		frida-tools-update-submodule-stamp
.SECONDARY:
