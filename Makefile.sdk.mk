include config.mk
include releng/deps.mk


ifeq ($(FRIDA_V8), auto)
FRIDA_V8 := $(shell echo $(host_machine) | grep -Evq "^(linux-mips|qnx-)" && echo "enabled" || echo "disabled")
endif


packages = \
	zlib \
	xz \
	brotli \
	minizip \
	sqlite \
	libffi \
	pcre2 \
	glib \
	glib-networking \
	libnice \
	usrsctp \
	libgee \
	json-glib \
	libxml2 \
	libsoup \
	capstone \
	quickjs \
	$(NULL)


ifeq ($(host_os), $(filter $(host_os), macos ios watchos tvos))
# Pull in iconv so our payloads only depend on libSystem.
glib_deps += libiconv
endif

ifeq ($(host_os), $(filter $(host_os), linux android qnx))
packages += elfutils libdwarf libunwind
endif

ifeq ($(host_os), freebsd)
packages += libdwarf libunwind
endif

ifeq ($(host_os), android)
packages += selinux
endif

ifneq ($(FRIDA_V8), disabled)
packages += v8
ifeq ($(host_os), $(filter $(host_os), macos ios watchos tvos))
ifeq ($(FRIDA_ASAN), no)
packages += libcxx
endif
endif
endif

ifeq ($(host_arch), $(filter $(host_arch), x86 x86_64 arm armbe8 armeabi armhf arm64 arm64e arm64eoabi))
packages += tinycc
endif


.PHONY: all clean distclean

all: build/sdk-$(host_machine).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your SDK: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""

clean: $(foreach pkg, $(call expand-packages,$(packages)), clean-$(pkg))

distclean: $(foreach pkg, $(call expand-packages,$(packages)), distclean-$(pkg))


build/sdk-$(host_machine).tar.bz2: build/fs-tmp-$(host_machine)/.package-stamp
	@$(call print-status,📦,Compressing)
	@tar \
		-C build/fs-tmp-$(host_machine)/package \
		-cjf $(shell pwd)/$@.tmp \
		.
	@mv $@.tmp $@

build/fs-tmp-%/.package-stamp: $(foreach pkg, $(packages), build/fs-%/manifest/$(pkg).pkg)
	@echo
	@$(call print-status,📦,Assembling)
	@$(RM) -r $(@D)/package
	@mkdir -p $(@D)/package
	@cd build/fs-$*; \
		programs=""; \
		[ -d bin/$(host_os_arch) ] && programs=bin/$(host_os_arch); \
		[ -d bin/$(build_os_arch) ] && programs=bin/$(build_os_arch); \
		[ -d lib/tcc ] && tcc=lib/tcc || tcc=""; \
		[ -d lib/c++ ] && libcpp=lib/c++/*.a || libcpp=""; \
		[ -d lib/gio/modules ] && gio_modules=lib/gio/modules/*.a || gio_modules=""; \
		[ -d lib32 ] && lib32=lib32 || lib32=""; \
		[ -d lib64 ] && lib64=lib64 || lib64=""; \
		[ -d libdata ] && libdatadir=libdata || libdatadir=lib; \
		tar -cf - \
			$$programs \
			include \
			lib/*.a \
			lib/glib-2.0 \
			lib/libffi* \
			$$libdatadir/pkgconfig \
			$$tcc \
			$$libcpp \
			$$gio_modules \
			$$lib32 \
			$$lib64 \
			manifest \
			share/glib-2.0/schemas \
			share/vala \
			| tar -C $(shell pwd)/$(@D)/package -xf -
	@releng/pkgify.sh "$(@D)/package" "$(shell pwd)/build/fs-$*" "$(shell pwd)/releng"
	@echo "$(frida_deps_version)" > $(@D)/package/VERSION.txt
	@touch $@


$(eval $(call make-package-rules,$(packages),fs))


build/fs-env-%.rc:
	@if [ $* != $(build_machine) ]; then \
		cross=yes; \
	else \
		cross=no; \
	fi; \
	for os_arch in $(build_machine) $*; do \
		if [ ! -f build/fs-env-$$os_arch.rc ]; then \
			FRIDA_HOST=$$os_arch \
			FRIDA_CROSS=$$cross \
			FRIDA_ASAN=$(FRIDA_ASAN) \
			FRIDA_ENV_NAME=fs \
			FRIDA_ENV_SDK=none \
			FRIDA_TOOLCHAIN_VERSION=$(frida_bootstrap_version) \
			XCODE11="$(XCODE11)" \
			./releng/setup-env.sh || exit 1; \
		fi \
	done

releng/meson/meson.py:
	git submodule init releng/meson
	git submodule update releng/meson
	@touch $@


.SECONDARY:
