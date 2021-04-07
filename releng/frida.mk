FRIDA_VERSION := $(shell git describe --tags --always --long | sed 's,-,.,g' | cut -f1-3 -d'.')

include releng/system.mk

FOR_HOST ?= $(build_os_arch)

frida_gum_flags := --default-library static $(FRIDA_MESONFLAGS_COMMON) -Dv8=$(FRIDA_V8)
frida_core_flags := --default-library static $(FRIDA_MESONFLAGS_COMMON) $(FRIDA_MAPPER_FLAGS)

frida_tools = frida frida-discover frida-kill frida-ls-devices frida-ps frida-trace

v8_api_version = 8.0

build/frida-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ACOPTFLAGS="$(FRIDA_ACOPTFLAGS_COMMON)" \
			FRIDA_ACDBGFLAGS="$(FRIDA_ACDBGFLAGS_COMMON)" \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
build/frida_thin-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida_thin-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ACOPTFLAGS="$(FRIDA_ACOPTFLAGS_COMMON)" \
			FRIDA_ACDBGFLAGS="$(FRIDA_ACDBGFLAGS_COMMON)" \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=frida_thin \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
	@cd $(FRIDA)/build/; \
	[ ! -e frida-env-$*.rc ] && ln -s frida_thin-env-$*.rc frida-env-$*.rc; \
	[ ! -e frida-meson-env-$*.rc ] && ln -s frida_thin-meson-env-$*.rc frida-meson-env-$*.rc; \
	[ ! -d frida-$* ] && ln -s frida_thin-$* frida-$*; \
	[ ! -d sdk-$* ] && ln -s frida_thin-sdk-$* sdk-$*; \
	[ ! -d toolchain-$* ] && ln -s frida_thin-toolchain-$* toolchain-$*; \
	true
build/frida_gir-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida_gir-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ACOPTFLAGS="$(FRIDA_ACOPTFLAGS_COMMON)" \
			FRIDA_ACDBGFLAGS="$(FRIDA_ACDBGFLAGS_COMMON)" \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=frida_gir \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
	@cd $(FRIDA)/build/; \
	[ ! -e frida-env-$*.rc ] && ln -s frida_gir-env-$*.rc frida-env-$*.rc; \
	[ ! -e frida-meson-env-$*.rc ] && ln -s frida_gir-meson-env-$*.rc frida-meson-env-$*.rc; \
	[ ! -d frida-$* ] && ln -s frida_gir-$* frida-$*; \
	[ ! -d sdk-$* ] && ln -s frida_gir-sdk-$* sdk-$*; \
	[ ! -d toolchain-$* ] && ln -s frida_gir-toolchain-$* toolchain-$*; \
	true

build/frida-version.h: releng/generate-version-header.py .git/refs/heads/master
	@$(PYTHON3) releng/generate-version-header.py > $@.tmp
	@mv $@.tmp $@
