FRIDA_VERSION := $(shell git describe --tags --always --long | sed 's,-,.,g' | cut -f1-3 -d'.')

build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
build_arch := $(shell releng/detect-arch.sh)
build_platform_arch := $(build_platform)-$(build_arch)

FOR_HOST ?= $(build_platform_arch)

frida_gum_flags := --default-library static $(FRIDA_COMMON_FLAGS)
frida_core_flags := --default-library static $(FRIDA_COMMON_FLAGS) $(FRIDA_MAPPER_FLAGS)

frida_tools := frida frida-discover frida-kill frida-ls-devices frida-ps frida-trace

v8_api_version := 7.0

build/frida-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	FRIDA_HOST=$* \
		FRIDA_OPTIMIZATION_FLAGS="$(FRIDA_OPTIMIZATION_FLAGS)" \
		FRIDA_DEBUG_FLAGS="$(FRIDA_DEBUG_FLAGS)" \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		./releng/setup-env.sh
build/frida_thin-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	FRIDA_HOST=$* \
		FRIDA_OPTIMIZATION_FLAGS="$(FRIDA_OPTIMIZATION_FLAGS)" \
		FRIDA_DEBUG_FLAGS="$(FRIDA_DEBUG_FLAGS)" \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		FRIDA_ENV_NAME=frida_thin \
		./releng/setup-env.sh
	@cd $(FRIDA)/build/; \
	[ ! -e frida-env-$*.rc ] && ln -s frida_thin-env-$*.rc frida-env-$*.rc; \
	[ ! -e frida-meson-env-$*.rc ] && ln -s frida_thin-meson-env-$*.rc frida-meson-env-$*.rc; \
	[ ! -d frida-$* ] && ln -s frida_thin-$* frida-$*; \
	[ ! -d sdk-$* ] && ln -s frida_thin-sdk-$* sdk-$*; \
	[ ! -d toolchain-$* ] && ln -s frida_thin-toolchain-$* toolchain-$*; \
	true

build/frida-version.h: releng/generate-version-header.py .git/refs/heads/master
	@python releng/generate-version-header.py > $@.tmp
	@mv $@.tmp $@

glib:
	@if [ ! -e build/fs-$(FOR_HOST)/lib/pkgconfig/glib-2.0.pc ]; then \
		make -f Makefile.sdk.mk FRIDA_HOST=$(FOR_HOST) build/fs-$(FOR_HOST)/lib/pkgconfig/glib-2.0.pc; \
	else \
		. build/fs-meson-env-$(build_platform_arch).rc && $(NINJA) -C build/fs-tmp-$(FOR_HOST)/glib; \
	fi
glib-symlinks:
	@cd build; \
	for candidate in $$(find . -mindepth 1 -maxdepth 1 -name "sdk-*"); do \
		host_arch=$$(echo $$candidate | cut -f2- -d"-"); \
		if [ -d "fs-tmp-$$host_arch/glib" ]; then \
			echo "✓ $$host_arch"; \
			rm -rf sdk-$$host_arch/include/glib-2.0 sdk-$$host_arch/include/gio-unix-2.0 sdk-$$host_arch/lib/glib-2.0; \
			ln -s ../../fs-$$host_arch/include/glib-2.0 sdk-$$host_arch/include/glib-2.0; \
			ln -s ../../fs-$$host_arch/include/gio-unix-2.0 sdk-$$host_arch/include/gio-unix-2.0; \
			ln -s ../../fs-$$host_arch/lib/glib-2.0 sdk-$$host_arch/lib/glib-2.0; \
			for name in glib gthread gmodule gobject gio; do \
				libname=lib$$name-2.0.a; \
				rm -f sdk-$$host_arch/lib/$$libname; \
				ln -s ../../fs-tmp-$$host_arch/glib/$$name/$$libname sdk-$$host_arch/lib/$$libname; \
				pcname=$$name-2.0.pc; \
				rm -f sdk-$$host_arch/lib/pkgconfig/$$pcname; \
				ln -s ../../../fs-$$host_arch/lib/pkgconfig/$$pcname sdk-$$host_arch/lib/pkgconfig/$$pcname; \
			done; \
			for name in gmodule-export gmodule-no-export gio-unix; do \
				pcname=$$name-2.0.pc; \
				rm -f sdk-$$host_arch/lib/pkgconfig/$$pcname; \
				ln -s ../../../fs-$$host_arch/lib/pkgconfig/$$pcname sdk-$$host_arch/lib/pkgconfig/$$pcname; \
			done; \
			for name in glib-2.0.m4 glib-gettext.m4 gsettings.m4; do \
				rm -f sdk-$$host_arch/share/aclocal/$$name; \
				ln -s ../../../fs-$$host_arch/share/aclocal/$$name sdk-$$host_arch/share/aclocal/$$name; \
			done; \
			rm -rf sdk-$$host_arch/share/glib-2.0; \
			ln -s ../../fs-$$host_arch/share/glib-2.0 sdk-$$host_arch/share/glib-2.0; \
		fi; \
	done

v8:
	@if [ ! -e build/fs-$(FOR_HOST)/lib/pkgconfig/v8-$(v8_api_version).pc ]; then \
		make -f Makefile.sdk.mk FRIDA_HOST=$(FOR_HOST) build/fs-$(FOR_HOST)/lib/pkgconfig/v8-$(v8_api_version).pc; \
	else \
		$(NINJA) -C build/fs-tmp-$(FOR_HOST)/v8 v8_monolith; \
	fi
v8-symlinks:
	@cd build; \
	for candidate in $$(find . -mindepth 1 -maxdepth 1 -name "sdk-*"); do \
		host_arch=$$(echo $$candidate | cut -f2- -d"-"); \
		if [ -d "fs-tmp-$$host_arch/v8" ]; then \
			echo "✓ $$host_arch"; \
			rm -rf sdk-$$host_arch/include/v8-$(v8_api_version); \
			ln -s ../../fs-$$host_arch/include/v8-$(v8_api_version) sdk-$$host_arch/include/v8-$(v8_api_version); \
			rm -f sdk-$$host_arch/lib/libv8-$(v8_api_version).a; \
			ln -s ../../fs-tmp-$$host_arch/v8/obj/libv8_monolith.a sdk-$$host_arch/lib/libv8-$(v8_api_version).a; \
			pcname=v8-$(v8_api_version).pc; \
			rm -f sdk-$$host_arch/lib/pkgconfig/$$pcname; \
			ln -s ../../../fs-$$host_arch/lib/pkgconfig/$$pcname sdk-$$host_arch/lib/pkgconfig/$$pcname; \
		fi; \
	done
