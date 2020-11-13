include config.mk
include releng/dependencies.mk


MAKE_J ?= -j 8
SHELL := /bin/bash


all: build/toolchain-$(host_os)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your toolchain: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""


build/toolchain-$(host_os)-$(host_arch).tar.bz2: build/ft-tmp-$(host_os_arch)/.package-stamp
	tar \
		-C build/ft-tmp-$(host_os_arch)/package \
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
				$$STRIP $$f || exit 1; \
			fi; \
		done \
		&& $$STRIP $(@D)/package/lib/vala-*/gen-introspect-*
	releng/relocatify.sh $(@D)/package $(abspath build/ft-$*) $(abspath releng)
	@touch $@


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
		&& PATH=$$(shell pwd)/build/ft-$(build_os_arch)/bin:$$$$PATH $(MESON) \
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
		&& PATH=$$(shell pwd)/build/ft-$(build_os_arch)/bin:$$$$PATH $(NINJA) -C build/ft-tmp-$$*/$1 install)
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

$(eval $(call make-git-meson-module-rules,zlib,$(zlib_version),build/ft-%/lib/pkgconfig/zlib.pc,,$(zlib_options)))

$(eval $(call make-git-meson-module-rules,libffi,$(libffi_version),build/ft-%/lib/pkgconfig/libffi.pc,,$(libffi_options)))

$(eval $(call make-git-meson-module-rules,glib,$(glib_version),build/ft-%/bin/glib-genmarshal,build/ft-%/lib/pkgconfig/zlib.pc build/ft-%/lib/pkgconfig/libffi.pc,$(glib_options)))

$(eval $(call make-git-meson-module-rules,pkg-config,$(pkg_config_version),build/ft-%/bin/pkg-config,build/ft-%/bin/glib-genmarshal,$(pkg_config_options)))

$(eval $(call make-tarball-module-rules,flex,https://github.com/westes/flex/releases/download/v$(flex_version)/flex-$(flex_version).tar.gz,$(flex_hash),build/ft-%/bin/flex,build/ft-$(build_os_arch)/bin/m4,flex-modern-glibc.patch))

$(eval $(call make-tarball-module-rules,bison,https://$(gnu_mirror)/bison/bison-$(bison_version).tar.gz,$(bison_hash),build/ft-%/bin/bison,build/ft-$(build_os_arch)/bin/m4))

$(eval $(call make-git-meson-module-rules,vala,$(vala_version),build/ft-%/bin/valac,build/ft-%/bin/glib-genmarshal build/ft-$(build_os_arch)/bin/flex build/ft-$(build_os_arch)/bin/bison,$(vala_options)))


ifeq ($(host_os), $(filter $(host_os), macos ios))
	export_ldflags := -Wl,-exported_symbols_list,$(abspath build/ft-executable.symbols)
else
	export_ldflags := -Wl,--version-script,$(abspath build/ft-executable.version)
endif

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
