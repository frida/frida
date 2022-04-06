FRIDA_VERSION := $(shell git describe --tags --always --long | sed 's,-,.,g' | cut -f1-3 -d'.')

include releng/system.mk

FOR_HOST ?= $(build_os_arch)

frida_gum_flags := \
	--default-library static \
	$(FRIDA_FLAGS_COMMON) \
	-Dgumpp=enabled \
	-Dgumjs=enabled \
	-Dv8=$(FRIDA_V8) \
	-Ddatabase=$(FRIDA_DATABASE) \
	-Dfrida_objc_bridge=$(FRIDA_OBJC_BRIDGE) \
	-Dfrida_swift_bridge=$(FRIDA_SWIFT_BRIDGE) \
	-Dfrida_java_bridge=$(FRIDA_JAVA_BRIDGE) \
	-Dtests=enabled \
	$(NULL)
frida_core_flags := \
	--default-library static \
	$(FRIDA_FLAGS_COMMON) \
	-Dconnectivity=$(FRIDA_CONNECTIVITY) \
	$(FRIDA_MAPPER)

frida_tools = frida frida-discover frida-kill frida-ls-devices frida-ps frida-trace

v8_api_version = 8.0

build/frida-env-%.rc: releng/setup-env.sh build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
build/frida_thin-env-%.rc: releng/setup-env.sh build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida_thin-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=frida_thin \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
	@cd $(FRIDA)/build/; \
	[ ! -e frida-env-$*.rc ] && ln -s frida_thin-env-$*.rc frida-env-$*.rc; \
	[ ! -d frida-$* ] && ln -s frida_thin-$* frida-$*; \
	[ ! -d sdk-$* ] && ln -s frida_thin-sdk-$* sdk-$*; \
	[ ! -d toolchain-$* ] && ln -s frida_thin-toolchain-$* toolchain-$*; \
	true
build/frida_gir-env-%.rc: releng/setup-env.sh build/frida-version.h
	@for os_arch in $(build_os_arch) $*; do \
		if [ ! -f build/frida_gir-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=frida_gir \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done
	@cd $(FRIDA)/build/; \
	[ ! -e frida-env-$*.rc ] && ln -s frida_gir-env-$*.rc frida-env-$*.rc; \
	[ ! -d frida-$* ] && ln -s frida_gir-$* frida-$*; \
	[ ! -d sdk-$* ] && ln -s frida_gir-sdk-$* sdk-$*; \
	[ ! -d toolchain-$* ] && ln -s frida_gir-toolchain-$* toolchain-$*; \
	true

build/frida-version.h: releng/generate-version-header.py .git/HEAD
	@$(PYTHON3) releng/generate-version-header.py > $@.tmp
	@mv $@.tmp $@

define meson-setup
	$(call meson-setup-for-env,frida,$1)
endef

define meson-setup-thin
	$(call meson-setup-for-env,frida_thin,$1)
endef

define meson-setup-for-env
	meson_args="--native-file build/$1-$(build_os_arch).txt"; \
	if [ $2 != $(build_os_arch) ]; then \
		meson_args="$$meson_args --cross-file build/$1-$2.txt"; \
	fi; \
	$(MESON) setup $$meson_args
endef
