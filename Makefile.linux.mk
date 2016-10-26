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
	print "  \$$ make $${target_color}python-64 $${variable_color}PYTHON$${reset_color}=/opt/python35-64/bin/python3.5\n"; \
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
	rm -rf build/frida-android-arm64
	rm -rf build/frida-qnx-i386
	rm -rf build/frida-qnx-arm
	rm -rf build/frida-qnx-armeabi
	rm -rf build/frida_stripped-linux-i386
	rm -rf build/frida_stripped-linux-x86_64
	rm -rf build/frida_stripped-android-i386
	rm -rf build/frida_stripped-android-arm
	rm -rf build/frida_stripped-android-arm64
	rm -rf build/frida_stripped-qnx-i386
	rm -rf build/frida_stripped-qnx-arm
	rm -rf build/frida_stripped-qnx-armeabi
	rm -rf build/tmp-linux-i386
	rm -rf build/tmp-linux-x86_64
	rm -rf build/tmp-android-i386
	rm -rf build/tmp-android-arm
	rm -rf build/tmp-android-arm64
	rm -rf build/tmp-qnx-i386
	rm -rf build/tmp-qnx-arm
	rm -rf build/tmp-qnx-armeabi
	rm -rf build/tmp_stripped-linux-i386
	rm -rf build/tmp_stripped-linux-x86_64
	rm -rf build/tmp_stripped-android-i386
	rm -rf build/tmp_stripped-android-arm
	rm -rf build/tmp_stripped-android-arm64
	rm -rf build/tmp_stripped-qnx-i386
	rm -rf build/tmp_stripped-qnx-arm
	rm -rf build/tmp_stripped-qnx-armeabi
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
			*-armhf)  capstone_archs="arm"     ;; \
			*-armeabi)capstone_archs="arm"     ;; \
			*-arm64)  capstone_archs="aarch64" ;; \
			*-mips)   capstone_archs="mips"    ;; \
			*-mipsel) capstone_archs="mips"    ;; \
		esac \
		&& make -C capstone \
			PREFIX=$$frida_prefix \
			BUILDDIR=../build/tmp-$*/capstone \
			CAPSTONE_ARCHS="$$capstone_archs" \
			CAPSTONE_SHARED=$$enable_shared \
			CAPSTONE_STATIC=$$enable_static \
			install


gum-32: build/frida-linux-i386/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for i386
gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for x86-64
gum-android: build/frida-android-arm/lib/pkgconfig/frida-gum-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-gum-1.0.pc ##@gum Build for Android

build/frida-gum-autogen-stamp: build/frida-env-linux-$(build_arch).rc frida-gum/configure.ac
	@$(NPM) --version &>/dev/null || (echo -e "\033[31mOops. It appears Node.js is not installed.\nWe need it for processing JavaScript code at build-time.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
	. build/frida-env-linux-$(build_arch).rc && cd frida-gum && ./autogen.sh
	@touch -c $@

build/tmp-%/frida-gum/Makefile: build/frida-env-%.rc build/frida-gum-autogen-stamp build/frida-%/lib/pkgconfig/capstone.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-gum/configure

build/frida-%/lib/pkgconfig/frida-gum-1.0.pc: build/tmp-%/frida-gum/Makefile build/frida-gum-submodule-stamp
	@$(call ensure_relink,frida-gum/gum/gum.c,build/tmp-$*/frida-gum/gum/libfrida_gum_la-gum.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-gum install
	@touch -c $@

check-gum-32: build/frida-linux-i386/lib/pkgconfig/frida-gum-1.0.pc build/frida-gum-submodule-stamp ##@gum Run tests for i386
	build/tmp-linux-i386/frida-gum/tests/gum-tests $(test_args)
check-gum-64: build/frida-linux-x86_64/lib/pkgconfig/frida-gum-1.0.pc build/frida-gum-submodule-stamp ##@gum Run tests for x86-64
	build/tmp-linux-x86_64/frida-gum/tests/gum-tests $(test_args)


core-32: build/frida-linux-i386/lib/pkgconfig/frida-core-1.0.pc ##@core Build for i386
core-64: build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for x86-64
core-android: build/frida-android-arm/lib/pkgconfig/frida-core-1.0.pc build/frida-android-arm64/lib/pkgconfig/frida-core-1.0.pc ##@core Build for Android
core-qnx-arm: build/frida-qnx-arm/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX-arm
core-qnx-armeabi: build/frida-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc ##@core Build for QNX-armeabi
core-linux-mips: build/frida-linux-mips/lib/pkgconfig/frida-core-1.0.pc ##@core Build for mips
core-linux-mipsel: build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc ##@core Build for mipsel

frida-core/configure: build/frida-env-linux-$(build_arch).rc frida-core/configure.ac
	. build/frida-env-linux-$(build_arch).rc && cd frida-core && ./autogen.sh

build/tmp-%/frida-core/Makefile: build/frida-env-%.rc frida-core/configure build/frida-%/lib/pkgconfig/frida-gum-1.0.pc
	mkdir -p $(@D)
	. build/frida-env-$*.rc && cd $(@D) && ../../../frida-core/configure

build/frida-linux-i386/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-i386/frida-core/src/frida-helper build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-linux-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-i386.rc \
		&& cd build/tmp-linux-i386/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-i386/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper!frida-helper-64 \
			AGENT32=../../../../build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
		&& make install-data-am
	@touch -c $@
build/frida-linux-x86_64/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-i386/frida-core/src/frida-helper build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-linux-$*/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-x86_64.rc \
		&& cd build/tmp-linux-x86_64/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-i386/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-linux-x86_64/frida-core/src/frida-helper!frida-helper-64 \
			AGENT32=../../../../build/tmp_stripped-linux-i386/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-linux-x86_64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
		&& make install-data-am
	@touch -c $@
build/frida-linux-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-arm/frida-core/src/frida-helper build/tmp_stripped-linux-arm/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-linux-arm/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-arm.rc \
		&& cd build/tmp-linux-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-arm/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-linux-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-linux-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-linux-armhf/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-armhf/frida-core/src/frida-helper build/tmp_stripped-linux-armhf/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-linux-armhf/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-armhf/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-armhf.rc \
		&& cd build/tmp-linux-armhf/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-armhf/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-linux-armhf/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-linux-armhf/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-linux-mips/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-mips/frida-core/src/frida-helper build/tmp_stripped-linux-mips/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-linux-mips/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-mips/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-mips.rc \
		&& cd build/tmp-linux-mips/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-mips/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-linux-mips/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-linux-mips/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-linux-mipsel/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-linux-mipsel/frida-core/src/frida-helper build/tmp_stripped-linux-mipsel/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-linux-mipsel/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-mipsel/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-linux-mipsel.rc \
		&& cd build/tmp-linux-mipsel/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-linux-mipsel/frida-core/src/frida-helper!frida-helper-32 \
			LOADER32=../../../../build/tmp_stripped-linux-mipsel/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			AGENT32=../../../../build/tmp_stripped-linux-mipsel/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
		&& make install-data-am
	@touch -c $@
build/frida-android-i386/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-android-i386/frida-core/src/frida-helper build/tmp_stripped-android-i386/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-i386/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-android-i386/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-android-i386.rc \
		&& cd build/tmp-android-i386/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
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
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
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
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
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
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-android-arm64/frida-core/src/frida-helper!frida-helper-64 \
			LOADER32=../../../../build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			LOADER64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-64.so \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so \
		&& make install-data-am
	@touch -c $@
build/frida-qnx-arm/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-qnx-arm/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-qnx-arm/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-qnx-arm.rc \
		&& cd build/tmp-qnx-arm/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			AGENT=../../../../build/tmp_stripped-qnx-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent.so \
		&& make install-data-am
	@touch -c $@
build/frida-qnx-armeabi/lib/pkgconfig/frida-core-1.0.pc: build/tmp_stripped-qnx-armeabi/frida-core/lib/agent/.libs/libfrida-agent.so
	@$(call ensure_relink,frida-core/src/frida.c,build/tmp-qnx-armeabi/frida-core/src/libfrida_core_la-frida.lo)
	. build/frida-env-qnx-armeabi.rc \
		&& cd build/tmp-qnx-armeabi/frida-core \
		&& make -C src install \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			AGENT=../../../../build/tmp_stripped-qnx-armeabi/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent.so \
		&& make install-data-am
	@touch -c $@

build/tmp-%/frida-core/src/frida-helper: build/tmp-%/frida-core/lib/selinux/libfrida-selinux.stamp build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la
	@$(call ensure_relink,frida-core/src/darwin/frida-helper-glue.c,build/tmp-$*/frida-core/src/frida-helper-glue.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/src libfrida-helper-types.la frida-helper
	@touch -c $@
build/tmp_stripped-%/frida-core/src/frida-helper: build/tmp-%/frida-core/src/frida-helper
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
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

build/tmp-%/frida-core/lib/loader/libfrida-loader.la: build/tmp-%/frida-core/Makefile build/frida-core-submodule-stamp
	@$(call ensure_relink,frida-core/lib/loader/loader.c,build/tmp-$*/frida-core/lib/loader/libfrida_loader_la-loader.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/loader
	@touch -c $@

build/tmp_stripped-%/frida-core/lib/loader/.libs/libfrida-loader.so: build/tmp-%/frida-core/lib/loader/libfrida-loader.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/loader/.libs/libfrida-loader.so $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/lib/agent/libfrida-agent.la: build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la build/tmp-%/frida-core/lib/pipe/libfrida-pipe.la
	@$(call ensure_relink,frida-core/lib/agent/agent.c,build/tmp-$*/frida-core/lib/agent/libfrida_agent_la-agent.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/agent
	@touch -c $@

build/tmp_stripped-%/frida-core/lib/agent/.libs/libfrida-agent.so: build/tmp-%/frida-core/lib/agent/libfrida-agent.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/agent/.libs/libfrida-agent.so $@
	. build/frida-env-$*.rc && $$STRIP --strip-all $@

build/tmp-%/frida-core/lib/gadget/libfrida-gadget.la: build/tmp-%/frida-core/lib/interfaces/libfrida-interfaces.la
	@$(call ensure_relink,frida-core/lib/gadget/gadget.c,build/tmp-$*/frida-core/lib/gadget/libfrida_gadget_la-gadget.lo)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/lib/gadget
	@touch -c $@

build/frida_stripped-%/lib/frida-gadget.so: build/tmp-%/frida-core/lib/gadget/libfrida-gadget.la
	mkdir -p $(@D)
	cp build/tmp-$*/frida-core/lib/gadget/.libs/libfrida-gadget.so $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@

build/tmp-%/frida-core/tests/frida-tests: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/tests/main.c,build/tmp-$*/frida-core/tests/main.o)
	@$(call ensure_relink,frida-core/tests/inject-victim.c,build/tmp-$*/frida-core/tests/inject-victim.o)
	@$(call ensure_relink,frida-core/tests/inject-attacker.c,build/tmp-$*/frida-core/tests/inject-attacker.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/tests
	@touch -c $@

check-core-32: build/tmp-linux-i386/frida-core/tests/frida-tests build/frida-core-submodule-stamp ##@core Run tests for i386
	$< $(test_args)
check-core-64: build/tmp-linux-x86_64/frida-core/tests/frida-tests build/frida-core-submodule-stamp ##@core Run tests for x86-64
	$< $(test_args)
check-core-android-arm64: build/tmp_stripped-android-arm/frida-core/src/frida-helper build/tmp_stripped-android-arm64/frida-core/src/frida-helper build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so
	. build/frida-env-android-arm64.rc \
		&& cd build/tmp-android-arm64/frida-core \
		&& make check \
			RESOURCE_COMPILER="\"$(FRIDA)/releng/resource-compiler-linux-$(build_arch)\" --toolchain=gnu" \
			HELPER32=../../../../build/tmp_stripped-android-arm/frida-core/src/frida-helper!frida-helper-32 \
			HELPER64=../../../../build/tmp_stripped-android-arm64/frida-core/src/frida-helper!frida-helper-64 \
			LOADER32=../../../../build/tmp_stripped-android-arm/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-32.so \
			LOADER64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/loader/.libs/libfrida-loader.so!frida-loader-64.so \
			AGENT32=../../../../build/tmp_stripped-android-arm/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-32.so \
			AGENT64=../../../../build/tmp_stripped-android-arm64/frida-core/lib/agent/.libs/libfrida-agent.so!frida-agent-64.so

server-32: build/frida_stripped-linux-i386/bin/frida-server ##@server Build for i386
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-i386/bin/frida-server $(BINDIST)/bin/frida-server-linux-32
server-64: build/frida_stripped-linux-x86_64/bin/frida-server ##@server Build for x86-64
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-x86_64/bin/frida-server $(BINDIST)/bin/frida-server-linux-64
server-arm: build/frida_stripped-linux-arm/bin/frida-server ##@server Build for arm
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-arm/bin/frida-server $(BINDIST)/bin/frida-server-linux-arm
server-armhf: build/frida_stripped-linux-armhf/bin/frida-server ##@server Build for arm
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-armhf/bin/frida-server $(BINDIST)/bin/frida-server-linux-armhf
server-mips: build/frida_stripped-linux-mips/bin/frida-server ##@server Build for mips
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-mips/bin/frida-server $(BINDIST)/bin/frida-server-linux-mips
server-mipsel: build/frida_stripped-linux-mipsel/bin/frida-server ##@server Build for mipsel
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-mipsel/bin/frida-server $(BINDIST)/bin/frida-server-linux-mipsel
server-android: build/frida_stripped-android-arm/bin/frida-server build/frida_stripped-android-arm64/bin/frida-server ##@server Build for Android
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-android-arm/bin/frida-server $(BINDIST)/bin/frida-server-android
	cp -f build/frida_stripped-android-arm64/bin/frida-server $(BINDIST)/bin/frida-server-android64
server-qnx-arm: build/frida_stripped-qnx-arm/bin/frida-server ##@server Build for QNX-arm
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-qnx-arm/bin/frida-server $(BINDIST)/bin/frida-server-qnx-arm
server-qnx-armeabi: build/frida_stripped-qnx-armeabi/bin/frida-server ##@server Build for QNX-armeabi
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-qnx-armeabi/bin/frida-server $(BINDIST)/bin/frida-server-qnx-armeabi

inject-32: build/frida_stripped-linux-i386/bin/frida-inject ##@inject Build for i386
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-i386/bin/frida-inject $(BINDIST)/bin/frida-inject-linux-32
inject-64: build/frida_stripped-linux-x86_64/bin/frida-inject ##@inject Build for x86-64
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-x86_64/bin/frida-inject $(BINDIST)/bin/frida-inject-linux-64
inject-arm: build/frida_stripped-linux-arm/bin/frida-inject ##@inject Build for arm
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-arm/bin/frida-inject $(BINDIST)/bin/frida-inject-linux-arm
inject-armhf: build/frida_stripped-linux-armhf/bin/frida-inject ##@inject Build for armhf
	mkdir -p $(BINDIST)/bin
	cp -f build/frida_stripped-linux-armhf/bin/frida-inject $(BINDIST)/bin/frida-inject-linux-armhf

gadget-32: build/frida_stripped-linux-i386/lib/frida-gadget.so ##@gadget Build for i386
	mkdir -p $(BINDIST)/lib
	cp -f $< $(BINDIST)/lib/frida-gadget-32.so
gadget-64: build/frida_stripped-linux-x86_64/lib/frida-gadget.so ##@gadget Build for x86-64
	mkdir -p $(BINDIST)/lib
	cp -f $< $(BINDIST)/lib/frida-gadget-64.so
gadget-arm: build/frida_stripped-linux-arm/lib/frida-gadget.so ##@gadget Build for linux-arm
	mkdir -p $(BINDIST)/lib
	cp -f $< $(BINDIST)/lib/frida-gadget-arm.so
gadget-armhf: build/frida_stripped-linux-armhf/lib/frida-gadget.so ##@gadget Build for linux-armhf
	mkdir -p $(BINDIST)/lib
	cp -f $< $(BINDIST)/lib/frida-gadget-armhf.so
gadget-mipsel: build/frida_stripped-linux-mipsel/lib/frida-gadget.so ##@gadget Build for mipsel
	mkdir -p $(BINDIST)/lib
	cp -f $< $(BINDIST)/lib/frida-gadget-mipsel.so

build/frida_stripped-%/bin/frida-server: build/frida-%/bin/frida-server
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida-%/bin/frida-server: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/server/server.c,build/tmp-$*/frida-core/server/frida_server-server.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/server install
	@touch -c $@

build/frida_stripped-%/bin/frida-inject: build/frida-%/bin/frida-inject
	mkdir -p $(@D)
	cp $< $@.tmp
	. build/frida-env-$*.rc && $$STRIP --strip-all $@.tmp
	mv $@.tmp $@
build/frida-%/bin/frida-inject: build/frida-%/lib/pkgconfig/frida-core-1.0.pc
	@$(call ensure_relink,frida-core/inject/inject.c,build/tmp-$*/frida-core/inject/frida_inject-inject.o)
	. build/frida-env-$*.rc && make -C build/tmp-$*/frida-core/inject install
	@touch -c $@


python-32: build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-python-submodule-stamp ##@python Build Python bindings for i386
	mkdir -p $(BINDIST)/lib32/$(PYTHON_NAME)/site-packages
	cp -rf build/frida_stripped-linux-i386/lib/$(PYTHON_NAME)/* $(BINDIST)/lib32/$(PYTHON_NAME)/site-packages

python-64: build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/frida build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/site-packages/_frida.so build/frida-python-submodule-stamp ##@python Build Python bindings for x86-64
	mkdir -p $(BINDIST)/lib64/$(PYTHON_NAME)/site-packages
	cp -rf build/frida_stripped-linux-x86_64/lib/$(PYTHON_NAME)/* $(BINDIST)/lib64/$(PYTHON_NAME)/site-packages

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
	mkdir -p $(BINDIST)/lib32/node_modules
	cp -rf build/frida_stripped-linux-i386/lib/node_modules/frida $(BINDIST)/lib32/node_modules

node-64: build/frida_stripped-linux-x86_64/lib/node_modules/frida build/frida-node-submodule-stamp ##@node Build Node.js bindings for x86-64
	mkdir -p $(BINDIST)/lib64/node_modules
	cp -rf build/frida_stripped-linux-x86_64/lib/node_modules/frida $(BINDIST)/lib64/node_modules

build/frida_stripped-%/lib/node_modules/frida: build/frida-%/lib/pkgconfig/frida-core-1.0.pc build/frida-node-submodule-stamp
	@$(NPM) --version &>/dev/null || (echo -e "\033[31mOops. It appears Node.js is not installed.\nCheck PATH or set NODE to the absolute path of your Node.js binary.\033[0m"; exit 1;)
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
	server-32 server-64 server-android server-qnx-arm server-qnx-armeabi \
	python-32 python-64 check-python-32 check-python-64 frida-python-update-submodule-stamp \
	node-32 node-64 check-node-32 check-node-64 frida-node-update-submodule-stamp
.SECONDARY:
