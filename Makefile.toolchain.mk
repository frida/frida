include config.mk
include releng/deps.mk


packages = \
	ninja \
	termux-elf-cleaner \
	zlib \
	libffi \
	glib \
	pkg-config \
	vala \
	$(NULL)


ifeq ($(host_os), $(filter $(host_os), macos ios))
export_ldflags := -Wl,-exported_symbols_list,$(shell pwd)/build/ft-executable.symbols
else
export_ldflags := -Wl,--version-script,$(shell pwd)/build/ft-executable.version
endif

frida_env_config := \
	FRIDA_EXTRA_LDFLAGS="$(export_ldflags)" \
	FRIDA_ASAN=$(FRIDA_ASAN) \
	FRIDA_ENV_NAME=ft \
	FRIDA_ENV_SDK=none \
	FRIDA_TOOLCHAIN_VERSION=$(frida_bootstrap_version) \
	XCODE11="$(XCODE11)"


.PHONY: all clean distclean

all: build/toolchain-$(host_machine).tar.bz2
	@echo ""
	@echo -e "\\033[0;32mSuccess"'!'"\\033[0;39m Here's your toolchain: \\033[1m$<\\033[0m"
	@echo ""
	@if [ $$host_machine = $$build_machine ]; then \
		echo "It will be picked up automatically if you now proceed to build Frida."; \
		echo ""; \
	fi

clean: $(foreach pkg, $(call expand-packages,$(packages)), clean-$(pkg))

distclean: $(foreach pkg, $(call expand-packages,$(packages)), distclean-$(pkg))

build/toolchain-$(host_machine).tar.bz2: build/ft-tmp-$(host_machine)/.package-stamp
	@$(call print-status,📦,Compressing)
	@tar \
		-C build/ft-tmp-$(host_machine)/package \
		-cjf $(shell pwd)/$@.tmp \
		.
	@if [ $$host_machine = $$build_machine ]; then \
		bootstrap_toolchain=_toolchain-$(host_machine).tar.bz2; \
			cd build \
			&& if [ -e $$bootstrap_toolchain ]; then \
				rm -f $$bootstrap_toolchain; \
				ln -s toolchain-$(host_machine).tar.bz2 $$bootstrap_toolchain; \
			fi; \
	fi
	@mv $@.tmp $@

build/ft-tmp-%/.package-stamp: build/ft-env-%.rc $(foreach pkg, $(packages), build/ft-%/manifest/$(pkg).pkg)
	@echo
	@$(call print-status,📦,Assembling)
	@$(RM) -r $(@D)/package
	@mkdir -p $(@D)/package
	@cd build/ft-$* \
		&& [ -d libdata ] && libdatadir=libdata || libdatadir=lib \
		&& tar -cf - \
			--exclude bin/gapplication \
			--exclude bin/gdbus \
			--exclude bin/gio \
			--exclude bin/gobject-query \
			--exclude bin/gsettings \
			--exclude bin/iconv \
			--exclude etc \
			--exclude include \
			--exclude "lib/lib*" \
			--exclude lib/glib-2.0 \
			--exclude $$libdatadir/pkgconfig \
			--exclude "lib/vala-*/*.a" \
			--exclude share/aclocal \
			--exclude share/bash-completion \
			--exclude share/doc \
			--exclude share/gdb \
			--exclude share/gettext \
			--exclude share/info \
			--exclude share/locale \
			--exclude share/man \
			--exclude share/vala \
			--exclude "*.pyc" \
			--exclude "*.pyo" \
			. | tar -C $(shell pwd)/$(@D)/package -xf -
	@rm -rf $(@D)/package/libdata
	@releng/pkgify.sh "$(@D)/package" "$(shell pwd)/build/ft-$*" "$(shell pwd)/releng"
	@echo "$(frida_deps_version)" > $(@D)/package/VERSION.txt
	@touch $@


$(eval $(call make-package-rules,$(packages),ft))


$(eval $(call make-base-package-rules,ninja,ft,$(host_machine)))

deps/.ninja-stamp:
	$(call grab-and-prepare,ninja)
	@touch $@

build/ft-%/manifest/ninja.pkg: build/ft-env-%.rc deps/.ninja-stamp
	@if [ $* != $(build_machine) ]; then \
		$(MAKE) -f Makefile.toolchain.mk \
			FRIDA_HOST=$(build_machine) \
			build/ft-$(build_machine)/manifest/ninja.pkg || exit 1; \
	fi
	@$(call print-status,ninja,Building for $*)
	@prefix=$(shell pwd)/build/ft-$*; \
	builddir=build/ft-tmp-$*/ninja; \
	native_ninja=$(shell pwd)/build/ft-$(build_machine)/bin/ninja; \
	$(RM) -r $$builddir; \
	mkdir -p build/ft-tmp-$* \
	&& cp -a deps/ninja $$builddir \
	&& (set -x \
		&& . $< \
		&& . <(./releng/machine_file.py to-env ./build/ft-$*.txt --flavor=cpp) \
		&& cd $$builddir \
		&& if $$CC --version | grep -q clang; then \
			optflags="-Oz"; \
		else \
			optflags="-Os"; \
		fi \
		&& sed -e "s,-O2,$$optflags,g" configure.py > configure.py.new \
		&& cat configure.py.new > configure.py \
		&& rm configure.py.new \
		&& args="" \
		&& if [ $* = $(build_machine) ]; then \
			args="--bootstrap"; \
		fi \
		&& $(PYTHON) ./configure.py \
			$$args \
			--platform=$$(echo $* | cut -f1 -d"-" | sed -e 's,^macos$$,darwin,') \
		&& if [ $* != $(build_machine) ]; then \
			"$$native_ninja" || exit 1; \
		fi \
		&& $$STRIP ninja \
		&& install -d $$prefix/bin \
		&& install -m 755 ninja $$prefix/bin \
	) >>$$builddir/build.log 2>&1 \
	&& $(call print-status,ninja,Generating manifest) \
	&& mkdir -p $(@D) \
	&& echo "bin/ninja" > $@


build/ft-env-%.rc: build/ft-executable.symbols build/ft-executable.version
	@if [ $* != $(build_machine) ]; then \
		cross=yes; \
	else \
		cross=no; \
	fi; \
	for machine in $(build_machine) $*; do \
		if [ ! -f build/ft-env-$$machine.rc ]; then \
			FRIDA_HOST=$$machine FRIDA_CROSS=$$cross $(frida_env_config) ./releng/setup-env.sh; \
			case $$? in \
				0) \
					;; \
				2) \
					if [ "$$machine" = "$(build_machine)" ]; then \
						MAKE=$(MAKE) ./releng/bootstrap-toolchain.sh $$machine || exit 1; \
						FRIDA_HOST=$$machine FRIDA_CROSS=$$cross $(frida_env_config) ./releng/setup-env.sh || exit 1; \
					else \
						exit 1; \
					fi \
					;; \
				*) \
					exit 1; \
			esac \
		fi \
	done

build/ft-executable.symbols:
	@mkdir -p $(@D)
	@echo "# No exported symbols." > $@

build/ft-executable.version:
	@mkdir -p $(@D)
	@( \
		echo "{"; \
		echo "  global:"; \
		echo "    # FreeBSD needs these two:"; \
		echo "    __progname;"; \
		echo "    environ;"; \
		echo ""; \
		echo "  local:"; \
		echo "    *;"; \
		echo "};" \
	) > $@


.SECONDARY:
