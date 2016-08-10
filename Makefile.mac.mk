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
	print "  \$$ make $${target_color}python-mac $${variable_color}PYTHON$${reset_color}=/usr/local/bin/python3.5\n"; \
	print "  \$$ make $${target_color}node-mac $${variable_color}NODE$${reset_color}=/usr/local/bin/node\n"; \
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
	rm -rf build/frida-mac-i386
	rm -rf build/frida-mac-x86_64
	rm -rf build/frida-mac-universal
	rm -rf build/frida-ios-universal
	rm -rf build/frida-ios-i386
	rm -rf build/frida-ios-x86_64
	rm -rf build/frida-ios-arm
	rm -rf build/frida-ios-arm64
	rm -rf build/frida-android-i386
	rm -rf build/frida-android-arm
	rm -rf build/frida-android-arm64
	rm -rf build/frida_stripped-mac-i386
	rm -rf build/frida_stripped-mac-x86_64
	rm -rf build/frida_stripped-android-i386
	rm -rf build/frida_stripped-android-arm
	rm -rf build/frida_stripped-android-arm64
	rm -rf build/tmp-mac-i386
	rm -rf build/tmp-mac-x86_64
	rm -rf build/tmp-mac-universal
	rm -rf build/tmp-ios-i386
	rm -rf build/tmp-ios-x86_64
	rm -rf build/tmp-ios-arm
	rm -rf build/tmp-ios-arm64
	rm -rf build/tmp-ios-universal
	rm -rf build/tmp-android-i386
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-android-arm64
	rm -rf build/tmp_stripped-mac-x86_64
	rm -rf build/tmp_stripped-ios-i386
	rm -rf build/tmp_stripped-ios-x86_64
	rm -rf build/tmp_stripped-ios-arm
	rm -rf build/tmp_stripped-ios-arm64
	rm -rf build/tmp_stripped-android-i386
	rm -rf build/tmp_stripped-android-arm
	rm -rf build/tmp_stripped-android-arm64
	rm -rf $(BINDIST)

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
		&& case $* in \
			*-i386)   capstone_archs="x86"     ;; \
			*-x86_64) capstone_archs="x86"     ;; \
			*-arm)    capstone_archs="arm"     ;; \
			*-arm64)  capstone_archs="aarch64" ;; \
		esac \
		&& make -C capstone \
			PREFIX=$$frida_prefix \
			BUILDDIR=../build/tmp-$*/capstone \
			CAPSTONE_ARCHS="$$capstone_archs" \
			CAPSTONE_SHARED=$$enable_shared \
			CAPSTONE_STATIC=$$enable_static \
			install


gum-mac: build/frida-mac-i386/lib/pkgconfig/frida-gum-1.0.pc build/frida-mac-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Mac
gum-ios: build/frida-ios-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for iOS
gum-android: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android

build/frida-gum-autogen-stamp: build/frida-env-mac-$(build_arch).rc frida-gum/configure.ac
	@$(NPM) --version &>/dev/null || (echo "\033[31mOops. It appears Node.js is not installed.\nWe need it for processing JavaScript code at build-time.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	. build/frida-env-mac-$(build_arch).rc && cd frida-gum && ./autogen.sh
	@touch -c $@

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc build/frida-gum-autogen-stamp build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum-mac: build/frida-mac-i386/lib/pkgconfig/frida-gum-1.0.pc build/frida-mac-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Run tests for Mac
	build/tmp-mac-i386/frida-gum/tests/gum-tests $(test_args)
	build/tmp-mac-x86_64/frida-gum/tests/gum-tests $(test_args)


core-mac: build/frida-mac-i386/lib/pkgconfig/frida-core-1.0.pc build/frida-mac-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Mac
core-ios: build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for iOS
core-android: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android

frida-core/configure: build/frida-env-mac-$(build_arch).rc frida-core/configure.ac
	. build/frida-env-mac-$(build_arch).rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/frida-mac-%/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-mac-x86_64/frida-core/src/frida-helper build/tmp-mac-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-mac-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-mac-$*.rc \
		&& cd build/tmp-mac-$*/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=apple" \
			HELPER=../../../../build/tmp_stripped-mac-x86_64/frida-core/src/frida-helper \
			LOADER=../../../../build/tmp-mac-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib!FridaLoader.dylib \
			AGENT=../../../../build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib!frida-agent.dylib \
		&& make install-data-am
	@touch -c $@
build/frida-ios-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-universal/frida-core/src/frida-helper build/tmp-ios-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-ios-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-ios-arm.rc \
		&& cd build/tmp-ios-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=apple" \
			HELPER=../../../../build/tmp-ios-universal/frida-core/src/frida-helper \
			LOADER=../../../../build/tmp-ios-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib!FridaLoader.dylib \
			AGENT=../../../../build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib!frida-agent.dylib \
		&& make install-data-am
	@touch -c $@
build/frida-ios-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp-ios-universal/frida-core/src/frida-helper build/tmp-ios-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-ios-arm64/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-ios-arm64.rc \
		&& cd build/tmp-ios-arm64/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=apple" \
			HELPER=../../../../build/tmp_stripped-ios-arm64/frida-core/src/frida-helper \
			LOADER=../../../../build/tmp-ios-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib!FridaLoader.dylib \
			AGENT=../../../../build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib!frida-agent.dylib \
		&& make install-data-am
	@touch -c $@
build/frida-android-i386/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-i386/frida-core/src/frida-helper build/tmp_stripped-android-i386/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-i386/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-i386.rc \
		&& cd build/tmp-android-i386/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-i386/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-android-i386/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-android-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-i386/frida-core/src/frida-helper build/tmp_stripped-android-x86_64/frida-core/src/frida-helper build/tmp_stripped-android-i386/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-x86_64/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-i386/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-x86_64.rc \
		&& cd build/tmp-android-x86_64/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-i386/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-android-x86_64/frida-core/src/frida-helper!frida-helper-64 \
			LOADER32=../../../../build/tmp_stripped-android-i386/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			LOADER64=../../../../build/tmp_stripped-android-x86_64/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-64.so \
			AGENT32=../../../../build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-android-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
		&& make install-data-am
	@touch -c $@
build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-arm/frida-core/src/frida-helper build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-arm.rc \
		&& cd build/tmp-android-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-arm/frida-core/src/frida-helper build/tmp_stripped-android-arm64/frida-core/src/frida-helper build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-arm64.rc \
		&& cd build/tmp-android-arm64/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-android-arm64/frida-core/src/frida-helper!frida-helper-64 \
			LOADER32=../../../../build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			LOADER64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-64.so \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
		&& make install-data-am
	@touch -c $@

build/tmp-mac-%/frida-core/src/frida-helper: build/tmp-mac-%/frida-core/lib/interfaces/libfrida-interfaces.la build/tmp-mac-%/frida-core/lib/pipe/libfrida-pipe.la build/tmp-mac-%/frida-core/lib/agent/libfrida-agent-container.la
	@$(call ensure_relink,frida-core/src/darwin/frida-helper-glue.c,build/tmp-mac-$*/frida-core/src/frida-helper-glue.lo)
	. build/frida-env-mac-$*.rc && make -C build/tmp-mac-$*/frida-core/src libfrida-helper-types.la frida-helper.stamp
	@touch -c $@
build/tmp-ios-%/frida-core/src/frida-helper: build/tmp-ios-%/frida-core/lib/interfaces/libfrida-interfaces.la build/tmp-ios-%/frida-core/lib/pipe/libfrida-pipe.la build/tmp-ios-%/frida-core/lib/agent/libfrida-agent-container.la
	@$(call ensure_relink,frida-core/src/darwin/frida-helper-glue.c,build/tmp-ios-$*/frida-core/src/frida-helper-glue.lo)
	. build/frida-env-ios-$*.rc && make -C build/tmp-ios-$*/frida-core/src libfrida-helper-types.la frida-helper.stamp
	@touch -c $@
build/tmp-android-%/frida-core/src/frida-helper: build/tmp-android-%/frida-core/lib/selinux/libfrida-selinux.stamp build/tmp-android-%/frida-core/lib/interfaces/libfrida-interfaces.la
	@$(call ensure_relink,frida-core/src/linux/frida-helper-glue.c,build/tmp-android-$*/frida-core/src/frida-helper-glue.lo)
	. build/frida-env-android-$*.rc && make -C build/tmp-android-$*/frida-core/src libfrida-helper-types.la frida-helper
	@touch -c $@
build/tmp_stripped-mac-x86_64/frida-core/src/frida-helper: build/tmp-mac-x86_64/frida-core/src/frida-helper
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-mac-x86_64.rc \
		&& $$STRIP -Sx $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" -i "re.frida.Helper" $@.tmp
	mv $@.tmp $@
build/tmp_stripped-ios-arm/frida-core/src/frida-helper: build/tmp-ios-arm/frida-core/src/frida-helper
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-ios-arm.rc \
		&& $$STRIP -Sx $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@
build/tmp_stripped-ios-arm64/frida-core/src/frida-helper: build/tmp-ios-arm64/frida-core/src/frida-helper
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP -Sx $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@
build/tmp-ios-universal/frida-core/src/frida-helper: build/tmp_stripped-ios-arm/frida-core/src/frida-helper build/tmp_stripped-ios-arm64/frida-core/src/frida-helper
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	. build/frida-env-ios-arm64.rc \
		&& $$LIPO $^ -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" --entitlements frida-core/src/darwin/frida-helper.xcent $@.tmp
	mv $@.tmp $@
build/tmp_stripped-android-%/frida-core/src/frida-helper: build/tmp-android-%/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/lib/selinux/libfrida-selinux.stamp: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/pipe/pipe-posix.c,build/tmp-$*/frida-core/lib/pipe/libfrida_pipe_la-pipe-posix.lo)
	@$(call ensure_relink,frida-core/src/frida-glue.c,build/tmp-$*/frida-core/src/libfrida_core_glue_la-frida-glue.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/selinux
	@touch -c $@

build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/interfaces/session.c,build/tmp-$*/frida-core/lib/interfaces/libfrida_interfaces_la-session.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/interfaces
	@touch -c $@

build/tmp-%/frida-core/lib/pipe/libfrida-pipe.la: build/tmp-%/frida-core/lib/selinux/libfrida-selinux.stamp build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/pipe/pipe.c,build/tmp-$*/frida-core/lib/pipe/libfrida_pipe_la-pipe.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/pipe
	@touch -c $@

build/tmp-%/frida-core/lib/agent/libfrida-agent-container.la: build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la build/tmp-%/frida-core/lib/pipe/libfrida-pipe.la
	@$(call ensure_relink,frida-core/lib/agent/agent-container.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_container_la-agent-container.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/agent libfrida-agent-container.la
	@touch -c $@

build/tmp-%/frida-core/lib/loader/libfrida-loader.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/loader/loader.c,build/tmp-$*/frida-core/lib/loader/libfrida_loader_la-loader.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/loader
	@touch -c $@

build/tmp-mac-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib: build/tmp-mac-i386/frida-core/lib/loader/libfrida-loader.la build/tmp-mac-x86_64/frida-core/lib/loader/libfrida-loader.la
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-core/lib/loader/.libs/libfrida-loader.dylib $(@D)/libfrida-loader-32.dylib
	cp build/tmp-mac-x86_64/frida-core/lib/loader/.libs/libfrida-loader.dylib $(@D)/libfrida-loader-64.dylib
	. build/frida-env-mac-$(build_arch).rc \
		&& $$STRIP -Sx $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib \
		&& $$LIPO $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	rm $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib
	mv $@.tmp $@
build/tmp-ios-universal/frida-core/lib/loader/.libs/libfrida-loader.dylib: build/tmp-ios-arm/frida-core/lib/loader/libfrida-loader.la build/tmp-ios-arm64/frida-core/lib/loader/libfrida-loader.la
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-ios-arm/frida-core/lib/loader/.libs/libfrida-loader.dylib $(@D)/libfrida-loader-32.dylib
	cp build/tmp-ios-arm64/frida-core/lib/loader/.libs/libfrida-loader.dylib $(@D)/libfrida-loader-64.dylib
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP -Sx $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib \
		&& $$LIPO $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	rm $(@D)/libfrida-loader-32.dylib $(@D)/libfrida-loader-64.dylib
	mv $@.tmp $@
build/tmp_stripped-%/frida-core/lib/loader/.libs/libfrida-loader.so: build/tmp-%/frida-core/lib/loader/libfrida-loader.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/loader/.libs/libfrida-loader.so $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la build/tmp-%/frida-core/lib/pipe/libfrida-pipe.la
	@$(call ensure_relink,frida-core/lib/agent/agent.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_la-agent.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/agent
	@touch -c $@

build/tmp-mac-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-mac-i386/frida-core/lib/agent/libfrida-agent.la build/tmp-mac-x86_64/frida-core/lib/agent/libfrida-agent.la
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-32.dylib
	cp build/tmp-mac-x86_64/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-64.dylib
	. build/frida-env-mac-$(build_arch).rc \
		&& $$STRIP -Sx $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib \
		&& $$LIPO $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	rm $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib
	mv $@.tmp $@
build/tmp-ios-universal/frida-core/lib/agent/.libs/libfrida-agent.dylib: build/tmp-ios-arm/frida-core/lib/agent/libfrida-agent.la build/tmp-ios-arm64/frida-core/lib/agent/libfrida-agent.la
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-ios-arm/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-32.dylib
	cp build/tmp-ios-arm64/frida-core/lib/agent/.libs/libfrida-agent.dylib $(@D)/libfrida-agent-64.dylib
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP -Sx $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib \
		&& $$LIPO $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	rm $(@D)/libfrida-agent-32.dylib $(@D)/libfrida-agent-64.dylib
	mv $@.tmp $@
build/tmp_stripped-%/frida-core/lib/agent/.libs/libfrida-agent.so: build/tmp-%/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/agent/.libs/libfrida-agent.so $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/lib/gadget/libfrida-gadget.la: build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la
	@$(call ensure_relink,frida-core/lib/gadget/gadget.c,build/tmp-$*/frida-core/lib/gadget/libfrida_gadget_la-gadget.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/gadget
	@touch -c $@

build/frida-mac-universal/lib/FridaGadget.dylib: build/tmp-mac-i386/frida-core/lib/gadget/libfrida-gadget.la build/tmp-mac-x86_64/frida-core/lib/gadget/libfrida-gadget.la
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#mac-and-mac"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-i386.dylib
	cp build/tmp-mac-x86_64/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-x86_64.dylib
	. build/frida-env-mac-x86_64.rc \
		&& $$STRIP -Sx $(@D)/libfrida-gadget-i386.dylib $(@D)/libfrida-gadget-x86_64.dylib \
		&& $$LIPO $(@D)/libfrida-gadget-i386.dylib $(@D)/libfrida-gadget-x86_64.dylib -create -output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/../Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	rm $(@D)/libfrida-gadget-*.dylib
	mv $@.tmp $@
build/frida-ios-universal/lib/FridaGadget.dylib: build/tmp-ios-i386/frida-core/lib/gadget/libfrida-gadget.la build/tmp-ios-x86_64/frida-core/lib/gadget/libfrida-gadget.la build/tmp-ios-arm/frida-core/lib/gadget/libfrida-gadget.la build/tmp-ios-arm64/frida-core/lib/gadget/libfrida-gadget.la
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/tmp-ios-i386/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-i386.dylib
	cp build/tmp-ios-x86_64/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-x86_64.dylib
	cp build/tmp-ios-arm/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-armv7.dylib
	cp build/tmp-ios-arm64/frida-core/lib/gadget/.libs/libfrida-gadget.dylib $(@D)/libfrida-gadget-arm64.dylib
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP -Sx $(@D)/libfrida-gadget-i386.dylib $(@D)/libfrida-gadget-x86_64.dylib $(@D)/libfrida-gadget-armv7.dylib $(@D)/libfrida-gadget-arm64.dylib \
		&& $$LIPO $(@D)/libfrida-gadget-i386.dylib $(@D)/libfrida-gadget-x86_64.dylib $(@D)/libfrida-gadget-armv7.dylib $(@D)/libfrida-gadget-arm64.dylib -create -output $@.tmp \
		&& $$INSTALL_NAME_TOOL -id @executable_path/Frameworks/FridaGadget.dylib $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	rm $(@D)/libfrida-gadget-*.dylib
	mv $@.tmp $@
build/frida-android-%/lib/frida-gadget.so: build/tmp-android-%/frida-core/lib/gadget/libfrida-gadget.la
	mkdir -p $(@D)
	cp build/tmp-android-$*/frida-core/lib/gadget/.libs/libfrida-gadget.so $@.tmp
	. build/frida-env-android-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core-mac: build/tmp-mac-i386/frida-core/tests/frida-tests build/tmp-mac-x86_64/frida-core/tests/frida-tests ##@core Run tests for Mac
	build/tmp-mac-i386/frida-core/tests/frida-tests $(test_args)
	build/tmp-mac-x86_64/frida-core/tests/frida-tests $(test_args)
check-core-android-arm64: build/tmp_stripped-android-arm/frida-core/src/frida-helper build/tmp_stripped-android-arm64/frida-core/src/frida-helper build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so
	. build/frida-env-android-arm64.rc \
		&& cd build/tmp-android-arm64/frida-core \
		&& make check \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-mac-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-android-arm64/frida-core/src/frida-helper!frida-helper-64 \
			LOADER32=../../../../build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			LOADER64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-64.so \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so

server-mac: build/frida-mac-universal/bin/frida-server ##@server Build for Mac
	mkdir -p $(BINDIST)/bin
	cp -f build/frida-mac-universal/bin/frida-server $(BINDIST)/bin/frida-server-osx
server-ios: build/frida-ios-universal/bin/frida-server ##@server Build for iOS
	mkdir -p $(BINDIST)/bin
	cp -f build/frida-ios-universal/bin/frida-server $(BINDIST)/bin/frida-server-ios
server-android: build/frida_stripped-android-arm/bin/frida-server build/frida_stripped-android-arm64/bin/frida-server ##@server Build for Android
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-android-arm/bin/frida-server $(BINDIST)/bin/frida-server-android
	cp -f build/frida_stripped-android-arm64/bin/frida-server $(BINDIST)/bin/frida-server-android64

gadget-mac: build/frida-mac-universal/lib/FridaGadget.dylib ##@gadget Build for Mac
	mkdir -p $(BINDIST)/lib
	cp -f build/frida-mac-universal/lib/FridaGadget.dylib $(BINDIST)/lib/
gadget-ios: build/frida-ios-universal/lib/FridaGadget.dylib ##@gadget Build for iOS
	mkdir -p $(BINDIST)/lib
	cp -f build/frida-ios-universal/lib/FridaGadget.dylib $(BINDIST)/lib/
gadget-android: build/frida-android-arm/lib/frida-gadget.so build/frida-android-arm64/lib/frida-gadget.so ##@gadget Build for Android
	mkdir -p $(BINDIST)/lib
	cp -f build/frida-android-arm/lib/frida-gadget.so $(BINDIST)/lib/frida-gadget-android-arm.so
	cp -f build/frida-android-arm64/lib/frida-gadget.so $(BINDIST)/lib/frida-gadget-android-arm64.so

build/frida-mac-universal/bin/frida-server: build/frida-mac-i386/bin/frida-server build/frida-mac-x86_64/bin/frida-server
	@if [ -z "$$MAC_CERTID" ]; then echo "MAC_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/frida-mac-i386/bin/frida-server $(@D)/frida-server-32
	cp build/frida-mac-x86_64/bin/frida-server $(@D)/frida-server-64
	. build/frida-env-mac-$(build_arch).rc \
		&& $$STRIP -Sx $(@D)/frida-server-32 $(@D)/frida-server-64 \
		&& $$LIPO $(@D)/frida-server-32 $(@D)/frida-server-64 -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$MAC_CERTID" $@.tmp
	$(RM) $(@D)/frida-server-32 $(@D)/frida-server-64
	mv $@.tmp $@
build/frida-ios-universal/bin/frida-server: build/frida-ios-arm/bin/frida-server build/frida-ios-arm64/bin/frida-server
	@if [ -z "$$IOS_CERTID" ]; then echo "IOS_CERTID not set, see https://github.com/frida/frida#mac-and-ios"; exit 1; fi
	mkdir -p $(@D)
	cp build/frida-ios-arm/bin/frida-server $(@D)/frida-server-32
	cp build/frida-ios-arm64/bin/frida-server $(@D)/frida-server-64
	. build/frida-env-ios-arm64.rc \
		&& $$STRIP -Sx $(@D)/frida-server-32 $(@D)/frida-server-64 \
		&& $$LIPO $(@D)/frida-server-32 $(@D)/frida-server-64 -create -output $@.tmp \
		&& $$CODESIGN -f -s "$$IOS_CERTID" $@.tmp
	$(RM) $(@D)/frida-server-32 $(@D)/frida-server-64
	mv $@.tmp $@
build/frida_stripped-android-i386/bin/frida-server: build/frida-android-i386/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-i386.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida_stripped-android-x86_64/bin/frida-server: build/frida-android-x86_64/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-x86_64.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida_stripped-android-arm/bin/frida-server: build/frida-android-arm/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-arm.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida_stripped-android-arm64/bin/frida-server: build/frida-android-arm64/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-android-arm64.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida-%/bin/frida-server: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/server/server.c,build/tmp-$*/frida-core/server/frida_server-server.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/server install
	@touch -c $@


python-mac: build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/frida build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-mac-universal/bin/frida ##@python Build Python bindings for Mac
	mkdir -p $(BINDIST)/lib/$(PYTHON_NAME)/site-packages
	cp -rf build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/frida $(BINDIST)/lib/$(PYTHON_NAME)/site-packages

frida-python/configure: build/frida-env-mac-$(build_arch).rc frida-python/configure.ac
	. build/frida-env-mac-$(build_arch).rc && cd frida-python && ./autogen.sh

build/tmp-%/frida-$(PYTHON_NAME)/Makefile: build/frida-env-%.rc frida-python/configure build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && PYTHON=$(PYTHON) ../../../frida-python/configure

build/tmp-%/frida-$(PYTHON_NAME)/src/_frida.la: build/tmp-%/frida-$(PYTHON_NAME)/Makefile build/frida-python-submodule-stamp
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make
	@$(call ensure_relink,frida-python/src/_frida.c,build/tmp-$*/frida-$(PYTHON_NAME)/src/_frida.lo)
	. build/frida-env-$*.rc && cd build/tmp-$*/frida-$(PYTHON_NAME) && make install
	@touch -c $@

build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/frida: build/tmp-mac-x86_64/frida-$(PYTHON_NAME)/src/_frida.la
	rm -rf $@
	mkdir -p $(@D)
	cp -a build/frida-mac-x86_64/lib/$(PYTHON_NAME)/site-packages/frida $@
	@touch $@
build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/_frida.so: build/tmp-mac-i386/frida-$(PYTHON_NAME)/src/_frida.la build/tmp-mac-x86_64/frida-$(PYTHON_NAME)/src/_frida.la
	mkdir -p $(@D)
	cp build/tmp-mac-i386/frida-$(PYTHON_NAME)/src/.libs/_frida.so $(@D)/_frida-32.so
	cp build/tmp-mac-x86_64/frida-$(PYTHON_NAME)/src/.libs/_frida.so $(@D)/_frida-64.so
	. build/frida-env-mac-$(build_arch).rc \
		&& $$STRIP -Sx $(@D)/_frida-32.so $(@D)/_frida-64.so \
		&& $$LIPO $(@D)/_frida-32.so $(@D)/_frida-64.so -create -output $@
	rm $(@D)/_frida-32.so $(@D)/_frida-64.so

build/frida-mac-universal/bin/frida: build/tmp-mac-x86_64/frida-$(PYTHON_NAME)/src/_frida.la
	mkdir -p build/frida-mac-universal/bin \
		&& cp -r build/frida-mac-x86_64/bin/ build/frida-mac-universal/bin

check-python-mac: python-mac ##@python Test Python bindings for Mac
	export PYTHONPATH="$(shell pwd)/build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages" \
		&& cd frida-python \
		&& if $(PYTHON) -c "import sys; v = sys.version_info; can_execute_modules = v[0] > 2 or (v[0] == 2 and v[1] >= 7); sys.exit(0 if can_execute_modules else 1)"; then \
			$(PYTHON) -m unittest discover; \
		else \
			unit2 discover; \
		fi

install-python-mac: python-mac ##@python Install Python bindings for Mac
	sitepackages=`$(PYTHON) -c 'import site; print(site.getsitepackages()[0])'` \
		&& cp -r "build/frida-mac-universal/lib/$(PYTHON_NAME)/site-packages/" "$$sitepackages"

uninstall-python-mac: ##@python Uninstall Python bindings for mac
	cd `$(PYTHON) -c 'import site; print(site.getsitepackages()[0])'` \
		&& rm -rf _frida.so frida


node-mac: build/frida_stripped-mac-$(build_arch)/lib/node_modules/frida build/frida-node-submodule-stamp ##@node Build Node.js bindings for Mac
	mkdir -p $(BINDIST)/lib/node_modules/
	cp -rf build/frida_stripped-mac-$(build_arch)/lib/node_modules/frida $(BINDIST)/lib/node_modules/

build/frida_stripped-%/lib/node_modules/frida: build/frida-%/lib/pkgconfig/frida-core-1.0.pc build/frida-node-submodule-stamp
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
		&& . ../build/frida-env-mac-$(build_arch).rc && $$STRIP -Sx ../$@.tmp/build/frida_binding.node \
		&& mv ../$@.tmp ../$@

check-node-mac: build/frida_stripped-mac-$(build_arch)/lib/node_modules/frida ##@node Test Node.js bindings for Mac
	cd $< && $(NODE) --expose-gc node_modules/mocha/bin/_mocha


install-mac: install-python-mac ##@utilities Install frida utilities (frida{-discover,-ls-devices,-ps,-trace})
	@awk '/install_requires=\[/,/\],/' frida-python/src/setup.py | sed -n 's/.*"\(.*\)".*/\1/p' | $(PYTHON) -mpip install -r /dev/stdin \
		&& for b in "build/frida-mac-universal/bin"/*; do \
			n=`basename $$b`; \
			p="$(PREFIX)/bin/$$n"; \
			t=$$(mktemp -t frida); \
			grep -v 'sys.path.insert' "$$b" > "$$t"; \
			chmod +x "$$t"; \
			if [ -w "$(PREFIX)/bin" ]; then \
				mv "$$t" "$$p"; \
			else \
				sudo mv "$$t" "$$p"; \
			fi \
		done

uninstall-mac: ##@utilities Uninstall frida utilities
	@for n in frida frida-discover frida-ls-devices frida-ps frida-trace; do \
		if which "$$n" &> /dev/null; then \
			p=`which "$$n"`; \
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
	gum-mac gum-ios gum-android check-gum-mac frida-gum-update-submodule-stamp \
	core-mac core-ios core-android check-core-mac check-core-android-arm64 frida-core-update-submodule-stamp \
	server-mac server-ios server-android \
	gadget-mac gadget-ios gadget-android \
	python-mac check-python-mac install-python-mac uninstall-python-mac frida-python-update-submodule-stamp \
	node-mac check-node-mac frida-node-update-submodule-stamp \
	install-mac uninstall-mac
.SECONDARY:
