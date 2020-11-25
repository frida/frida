include config.mk
include releng/deps.mk


MAKE_J ?= -j 8
SHELL = /bin/bash


packages = \
	m4 \
	autoconf \
	automake \
	libtool \
	gettext \
	zlib \
	libffi \
	glib \
	pkg-config \
	flex \
	bison \
	vala \
	$(NULL)


.PHONY: all clean distclean

all: build/toolchain-$(host_os)-$(host_arch).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your toolchain: \\033[1m$<\\033[0m"
	@echo ""
	@echo "It will be picked up automatically if you now proceed to build Frida."
	@echo ""

clean: $(foreach pkg,$(packages),clean-$(pkg))

distclean: $(foreach pkg,$(packages),distclean-$(pkg))


build/toolchain-$(host_os)-$(host_arch).tar.bz2: build/ft-tmp-$(host_os_arch)/.package-stamp
	@$(call print-status,📦,Compressing)
	@tar \
		-C build/ft-tmp-$(host_os_arch)/package \
		-cjf $(abspath $@.tmp) \
		.
	@mv $@.tmp $@

build/ft-tmp-%/.package-stamp: build/ft-env-%.rc $(foreach pkg,$(packages),build/ft-%/manifest/$(pkg).pkg)
	@$(call print-status,📦,Assembling)
	@$(RM) -r $(@D)/package
	@mkdir -p $(@D)/package
	@cd build/ft-$* \
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
	@cd $(abspath $(@D)/package)/bin \
		&& for tool in aclocal automake; do \
			rm $$tool-$(automake_api_version); \
			mv $$tool $$tool-$(automake_api_version); \
			ln -s $$tool-$(automake_api_version) $$tool; \
		done
	@. $< \
		&& for f in $(@D)/manifest/bin/*; do \
			if [ -L $$f ]; then \
				true; \
			elif file -b --mime $$f | egrep -q "executable|binary"; then \
				$$STRIP $$f || exit 1; \
			fi; \
		done \
		&& $STRIP $(@D)/package/lib/vala-*/gen-introspect-*
	@releng/pkgify.sh $(@D)/package $(abspath build/ft-$*) $(abspath releng)
	@touch $@


define make-meson-package-rules
$(call make-meson-package-rules-for-env,$1,ft)
endef

define make-autotools-package-rules
$(call make-autotools-package-rules-for-env,$1,ft)
endef

$(eval $(call make-autotools-package-rules,libiconv))

$(eval $(call make-autotools-package-rules,m4))

$(eval $(call make-autotools-package-rules,autoconf))

$(eval $(call make-autotools-package-rules,automake))

.PHONY: libtool clean-libtool distclean-libtool

libtool: build/ft-$(host_os_arch)/bin/libtool

clean-libtool:
	[ -f build/ft-tmp-$(host_os_arch)/libtool/Makefile ] \
		&& $(MAKE) -C build/ft-tmp-$(host_os_arch)/libtool uninstall &>/dev/null || true
	$(call make-base-clean-commands,libtool,ft,$(host_os_arch))

distclean-libtool: clean-libtool
	$(call make-base-distclean-commands,libtool)

deps/.libtool-stamp:
	$(call grab-and-prepare,libtool)
	@cd deps/libtool \
		&& for name in aclocal.m4 config-h.in configure Makefile.in; do \
			find . -name $$name -exec touch '{}' \;; \
		done
	@touch $@

build/ft-tmp-%/libtool/Makefile: build/ft-env-%.rc deps/.libtool-stamp \
		$(foreach dep,$(libtool_deps),build/ft-%/manifest/$(dep).pkg)
	@$(call print-status,libtool,Configuring)
	@$(RM) -r $(@D)
	@mkdir -p $(@D)
	@(set -x \
		&& . $< \
		&& export PATH="$(shell pwd)/build/ft-$(build_os_arch)/bin:$$PATH" \
		&& cd $(@D) \
		&& ../../../deps/libtool/configure $(libtool_options) \
	) >$(@D)/build.log 2>&1

build/ft-%/$(libtool_target): build/ft-env-%.rc build/ft-tmp-%/libtool/Makefile
	@$(call print-status,libtool,Building)
	@(set -x \
		&& . $< \
		&& cd build/ft-tmp-$*/libtool \
		&& export PATH="$(shell pwd)/build/ft-$(build_os_arch)/bin:$$PATH" \
		&& $(MAKE) build-aux/ltmain.sh \
		&& touch ../../../deps/libtool/doc/*.1 ../../../deps/libtool/doc/stamp-vti \
		&& $(MAKE) $(MAKE_J) \
		&& $(MAKE) $(MAKE_J) install \
	) >>build/ft-tmp-$*/libtool/build.log 2>&1
	@touch $@

$(eval $(call make-autotools-manifest-rule,libtool,ft))

$(eval $(call make-autotools-package-rules,gettext))

$(eval $(call make-meson-package-rules,zlib))

$(eval $(call make-meson-package-rules,libffi))

$(eval $(call make-meson-package-rules,glib))

$(eval $(call make-meson-package-rules,pkg-config))

$(eval $(call make-autotools-package-rules,flex))

$(eval $(call make-autotools-package-rules,bison))

$(eval $(call make-meson-package-rules,vala))


ifeq ($(host_os), $(filter $(host_os), macos ios))
	export_ldflags := -Wl,-exported_symbols_list,$(abspath build/ft-executable.symbols)
else
	export_ldflags := -Wl,--version-script,$(abspath build/ft-executable.version)
endif

build/ft-env-%.rc: build/ft-executable.symbols build/ft-executable.version
	@FRIDA_HOST=$* \
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


.SECONDARY:
