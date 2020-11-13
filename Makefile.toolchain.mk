include config.mk

MAKE_J ?= -j 8

frida_toolchain_version := 20201106
frida_bootstrap_version := 20201028

repo_base_url = "https://github.com/frida"
repo_suffix = ".git"

m4_version := 1.4.18
m4_hash := ab2633921a5cd38e48797bf5521ad259bdc4b979078034a3b790d7fec5493fab
autoconf_version := 2.69
autoconf_hash := 954bd69b391edc12d6a4a51a2dd1476543da5c6bbf05a95b59dc0dd6fd4c2969
automake_version := 1.16.2
automake_api_version := 1.16
automake_hash := b2f361094b410b4acbf4efba7337bdb786335ca09eb2518635a09fb7319ca5c1
libtool_version := 2.4.6
libtool_hash := e3bd4d5d3d025a36c21dd6af7ea818a2afcd4dfc1ea5a17b39d7854bcd0c06e3
gettext_version := 0.21
gettext_hash := c77d0da3102aec9c07f43671e60611ebff89a996ef159497ce8e59d075786b12
zlib_version := 91920caec2160ffd919fd48dc4e7a0f6c3fb36d2
libffi_version := 4612f7f4b8cfda9a1f07e66d033bb9319860af9b
glib_version := 327fd7518d5612492723aec20c97fd2505e98fd8
pkg_config_version := b7fb5edc1f1a4fb17cd5cb94f4cf21912184da43
flex_version := 2.6.4
flex_hash := e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995
bison_version := 3.7.3
bison_hash := 104fe912f2212ab4e4a59df888a93b719a046ffc38d178e943f6c54b1f27b3c7
vala_version := 5067d99e7b9b8ab1c9393f70596fc5bd4f8b46a2

gnu_mirror := saimei.ftp.acc.umu.se/mirror/gnu.org/gnu


SHELL := /bin/bash

build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
build_arch := $(shell releng/detect-arch.sh)
build_platform_arch := $(build_platform)-$(build_arch)

ifdef FRIDA_HOST
	host_platform := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
else
	host_platform := $(build_platform)
endif
ifdef FRIDA_HOST
	host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
else
	host_arch := $(build_arch)
endif
host_platform_arch := $(host_platform)-$(host_arch)

ifeq ($(host_platform), macos)
	iconv := yes
endif
ifeq ($(host_platform), ios)
	iconv := yes
endif
ifeq ($(host_platform), android)
	iconv := yes
endif
ifeq ($(host_platform), qnx)
	iconv := yes
endif
ifeq ($(iconv),yes)
	glib_iconv_option := -Diconv=external
endif

ifeq ($(host_platform), linux)
	strip_all := --strip-all
endif
ifeq ($(host_platform), qnx)
	strip_all := --strip-all
endif
ifeq ($(host_platform), android)
	strip_all := --strip-all
endif
ifeq ($(host_platform), macos)
	strip_all := -Sx
endif
ifeq ($(host_platform), ios)
	strip_all := -Sx
endif

ifeq ($(host_platform),$(filter $(host_platform),macos ios))
	export_ldflags := -Wl,-exported_symbols_list,$(abspath build/ft-executable.symbols)
else
	export_ldflags := -Wl,--version-script,$(abspath build/ft-executable.version)
endif


all: build/toolchain-$(host_platform)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your toolchain: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""


build/toolchain-$(host_platform)-$(host_arch).tar.bz2: build/ft-tmp-$(host_platform_arch)/.package-stamp
	tar \
		-C build/ft-tmp-$(host_platform_arch)/package \
		-cjf $(abspath $@.tmp) \
		.
	mv $@.tmp $@

build/ft-tmp-%/.package-stamp: \
		build/ft-env-%.rc \
		build/ft-%/bin/m4 \
		build/ft-%/bin/autoconf \
		build/ft-%/bin/automake \
		build/ft-%/bin/libtool \
		build/ft-%/bin/autopoint \
		build/ft-%/bin/glib-genmarshal \
		build/ft-%/bin/pkg-config \
		build/ft-%/bin/valac
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/ft-$* \
		&& tar -c \
			--exclude bin/bison \
			--exclude bin/flex \
			--exclude bin/flex++ \
			--exclude bin/gapplication \
			--exclude bin/gdbus \
			--exclude bin/gio \
			--exclude bin/gio-launch-desktop \
			--exclude bin/gobject-query \
			--exclude bin/gsettings \
			--exclude bin/yacc \
			--exclude etc \
			--exclude include \
			--exclude lib/charset.alias \
			--exclude "lib/lib*" \
			--exclude lib/glib-2.0 \
			--exclude lib/gio \
			--exclude lib/pkgconfig \
			--exclude "lib/vala-*/*.a" \
			--exclude share/bash-completion \
			--exclude share/devhelp \
			--exclude share/doc \
			--exclude share/emacs \
			--exclude share/gdb \
			--exclude share/info \
			--exclude share/locale \
			--exclude share/man \
			--exclude share/vala/Makefile.vapigen \
			--exclude "*.pyc" \
			--exclude "*.pyo" \
			. | tar -C $(abspath $(@D)/package) -xf -
	cd $(abspath $(@D)/package)/bin \
		&& for tool in aclocal automake; do \
			rm $$tool-$(automake_api_version); \
			mv $$tool $$tool-$(automake_api_version); \
			ln -s $$tool-$(automake_api_version) $$tool; \
		done
	. $< \
		&& for f in $(@D)/package/bin/*; do \
			if [ -L $$f ]; then \
				true; \
			elif file -b --mime $$f | egrep -q "executable|binary"; then \
				$$STRIP $(strip_all) $$f || exit 1; \
			fi; \
		done \
		&& $$STRIP $(strip_all) $(@D)/package/lib/vala-*/gen-introspect-*
	releng/relocatify.sh $(@D)/package $(abspath build/ft-$*) $(abspath releng)
	@touch $@


define download-and-extract
	@$(RM) -r $3
	@mkdir -p $3
	@echo "[*] Downloading $1"
	@if command -v curl >/dev/null; then \
		curl -sSfLo $3/.tarball $1; \
	else \
		wget -qO $3/.tarball $1; \
	fi
	@echo "[*] Verifying checksum"
	@actual_checksum=$$(shasum -a 256 -b $3/.tarball | awk '{ print $$1; }'); \
	case $$actual_checksum in \
		$2) \
			;; \
		*) \
			echo "$1 checksum mismatch, expected=$2, actual=$$actual_checksum"; \
			exit 1; \
			;; \
	esac
	@echo "[*] Extracting to $3"
	@tar -x -f $3/.tarball -z -C $3 --strip-components 1
	@rm $3/.tarball
endef


define make-tarball-module-rules
build/.$1-stamp:
	$$(call download-and-extract,$2,$3,$1)
	@if [ -n "$6" ]; then \
		echo "[*] Applying patches"; \
		cd $1; \
		for patch in $6; do \
			patch -p1 < ../releng/patches/$$$$patch; \
		done; \
	fi
	@mkdir -p $$(@D)
	@touch $$@

build/ft-tmp-%/$1/Makefile: build/ft-env-%.rc build/.$1-stamp $5
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< \
		&& cd $$(@D) \
		&& PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH ../../../$1/configure

$4: build/ft-env-%.rc build/ft-tmp-%/$1/Makefile
	. $$< \
		&& cd build/ft-tmp-$$*/$1 \
		&& export PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums LN="ln -sf" install
	@touch $$@
endef

define make-git-meson-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone --recurse-submodules $(repo_base_url)/$1$(repo_suffix)
	cd $1 && git checkout -b $(frida_toolchain_version) $2
	@mkdir -p $$(@D)
	@touch $$@

build/ft-tmp-%/$1/build.ninja: build/ft-env-%.rc build/.$1-stamp $4 releng/meson/meson.py
	$(RM) -r $$(@D)
	(. build/ft-meson-env-$$*.rc \
		&& . build/ft-config-$$*.site \
		&& PATH=$$(shell pwd)/build/ft-$(build_platform_arch)/bin:$$$$PATH $(MESON) \
			--cross-file build/ft-$$*.txt \
			--prefix $$$$frida_prefix \
			--libdir $$$$frida_prefix/lib \
			--default-library static \
			$$(FRIDA_MESONFLAGS_BOTTLE) \
			$5 \
			$$(@D) \
			$1)

$3: build/ft-env-%.rc build/ft-tmp-%/$1/build.ninja
	(. $$< \
		&& PATH=$$(shell pwd)/build/ft-$(build_platform_arch)/bin:$$$$PATH $(NINJA) -C build/ft-tmp-$$*/$1 install)
	@touch $$@
endef

$(eval $(call make-tarball-module-rules,m4,https://$(gnu_mirror)/m4/m4-$(m4_version).tar.gz,$(m4_hash),build/ft-%/bin/m4,,m4-vasnprintf-apple-fix.patch m4-ftbfs-fix.patch))

$(eval $(call make-tarball-module-rules,autoconf,https://$(gnu_mirror)/autoconf/autoconf-$(autoconf_version).tar.gz,$(autoconf_hash),build/ft-%/bin/autoconf,build/ft-%/bin/m4,autoconf-uclibc.patch))

$(eval $(call make-tarball-module-rules,automake,https://$(gnu_mirror)/automake/automake-$(automake_version).tar.gz,$(automake_hash),build/ft-%/bin/automake,build/ft-%/bin/autoconf))

build/.libtool-stamp:
	$(call download-and-extract,https://$(gnu_mirror)/libtool/libtool-$(libtool_version).tar.gz,$(libtool_hash),libtool)
	@cd libtool \
		&& patch -p1 < ../releng/patches/libtool-fixes.patch \
		&& for name in aclocal.m4 config-h.in configure Makefile.in; do \
			find . -name $$name -exec touch '{}' \;; \
		done
	@mkdir -p $(@D)
	@touch $@

build/ft-tmp-%/libtool/Makefile: build/ft-env-%.rc build/.libtool-stamp build/ft-%/bin/automake
	$(RM) -r $(@D)
	mkdir -p $(@D)
	. $< && cd $(@D) && PATH=$(shell pwd)/build/ft-$*/bin:$$PATH ../../../libtool/configure

build/ft-%/bin/libtool: build/ft-env-%.rc build/ft-tmp-%/libtool/Makefile
	. $< \
		&& cd build/ft-tmp-$*/libtool \
		&& export PATH=$(shell pwd)/build/ft-$*/bin:$$PATH \
		&& make build-aux/ltmain.sh \
		&& touch ../../../libtool/doc/*.1 ../../../libtool/doc/stamp-vti \
		&& make $(MAKE_J) \
		&& make $(MAKE_J) install
	@touch $@

$(eval $(call make-tarball-module-rules,gettext,https://$(gnu_mirror)/gettext/gettext-$(gettext_version).tar.gz,$(gettext_hash),build/ft-%/bin/autopoint,build/ft-%/bin/libtool,gettext-static-only.patch))

$(eval $(call make-git-meson-module-rules,zlib,$(zlib_version),build/ft-%/lib/pkgconfig/zlib.pc,))

$(eval $(call make-git-meson-module-rules,libffi,$(libffi_version),build/ft-%/lib/pkgconfig/libffi.pc,,))

$(eval $(call make-git-meson-module-rules,glib,$(glib_version),build/ft-%/bin/glib-genmarshal,build/ft-%/lib/pkgconfig/zlib.pc build/ft-%/lib/pkgconfig/libffi.pc,$(glib_iconv_option) -Dselinux=disabled -Dxattr=false -Dlibmount=disabled -Dinternal_pcre=true -Dtests=false))

$(eval $(call make-git-meson-module-rules,pkg-config,$(pkg_config_version),build/ft-%/bin/pkg-config,build/ft-%/bin/glib-genmarshal,))

$(eval $(call make-tarball-module-rules,flex,https://github.com/westes/flex/releases/download/v$(flex_version)/flex-$(flex_version).tar.gz,$(flex_hash),build/ft-%/bin/flex,build/ft-$(build_platform_arch)/bin/m4,flex-modern-glibc.patch))

$(eval $(call make-tarball-module-rules,bison,https://$(gnu_mirror)/bison/bison-$(bison_version).tar.gz,$(bison_hash),build/ft-%/bin/bison,build/ft-$(build_platform_arch)/bin/m4))

$(eval $(call make-git-meson-module-rules,vala,$(vala_version),build/ft-%/bin/valac,build/ft-%/bin/glib-genmarshal build/ft-$(build_platform_arch)/bin/flex build/ft-$(build_platform_arch)/bin/bison,))


build/ft-env-%.rc: build/ft-executable.symbols build/ft-executable.version
	FRIDA_HOST=$* \
		FRIDA_ACOPTFLAGS="$(FRIDA_ACOPTFLAGS_BOTTLE)" \
		FRIDA_ACDBGFLAGS="$(FRIDA_ACDBGFLAGS_BOTTLE)" \
		FRIDA_EXTRA_LDFLAGS="$(export_ldflags)" \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		FRIDA_ENV_NAME=ft \
		FRIDA_ENV_SDK=none \
		FRIDA_TOOLCHAIN_VERSION=$(frida_bootstrap_version) \
		FRIDA_SDK_VERSION=$(frida_bootstrap_version) \
		./releng/setup-env.sh

build/ft-executable.symbols:
	@mkdir -p $(@D)
	@echo "# No exported symbols." > $@

build/ft-executable.version:
	@mkdir -p $(@D)
	@( \
		echo "FRIDA_TOOLCHAIN_EXECUTABLE {"; \
		echo "  local:"; \
		echo "    *;"; \
		echo "};" \
	) > $@


.PHONY: all
.SECONDARY:
