MAKE_J ?= -j 8
repo_base_url = "git://github.com/frida"
repo_suffix = ".git"

m4_version := 1.4.17
autoconf_version := 2.69
automake_version := 1.15
libtool_version := 2.4.6
pkg_config_version := 0.28


build_platform := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,mac,')
build_arch := $(shell uname -m)
build_platform_arch := $(build_platform)-$(build_arch)

ifeq ($(build_platform), linux)
download := wget -O - -q
else
download := curl -sS
endif

ifdef FRIDA_HOST
	host_platform := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
else
	host_platform := $(build_platform)
endif
ifdef FRIDA_HOST
	host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
else
	host_arch := $(shell uname -m)
endif
host_platform_arch := $(host_platform)-$(host_arch)

ifeq ($(host_platform), linux)
strip_all := --strip-all
endif
ifeq ($(host_platform), qnx)
strip_all := --strip-all
endif
ifeq ($(host_platform), android)
strip_all := --strip-all
endif
ifeq ($(host_platform), mac)
strip_all := -Sx
endif
ifeq ($(host_platform), ios)
strip_all := -Sx
endif


all: build/toolchain-$(host_platform)-$(host_arch).tar.bz2
	@echo ""
	@echo "\033[0;32mSuccess!\033[0;39m Here's your toolchain: \033[1m$<\033[0m"
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
		build/ft-tmp-%/.automake-symlink-stamp \
		build/ft-%/bin/libtool \
		build/ft-%/bin/glib-genmarshal \
		build/ft-%/bin/pkg-config \
		build/ft-%/bin/valac
	$(RM) -r $(@D)/package
	mkdir -p $(@D)/package
	cd build/ft-$* \
		&& tar -c \
			--exclude etc \
			--exclude include \
			--exclude lib \
			--exclude share/devhelp \
			--exclude share/doc \
			--exclude share/emacs \
			--exclude share/gdb \
			--exclude share/info \
			--exclude share/man \
			. | tar -C $(abspath $(@D)/package) -xf -
	. $< \
		&& for f in $(@D)/package/bin/*; do \
			if $$(file -b --mime-type $$f) | egrep -q "^application"; then \
				$$STRIP $(strip_all) $$f || exit 1 \
			fi \
		done
	releng/relocatify.sh $(@D)/package $(abspath build/ft-$*)
	@touch $@


define make-tarball-module-rules
build/.$1-stamp:
	$(RM) -r $1
	mkdir -p $1
	$(download) $2 | tar -C $1 -xz --strip-components 1
	@mkdir -p $$(@D)
	@touch $$@

build/ft-tmp-%/$1/Makefile: build/ft-env-%.rc build/.$1-stamp $4
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH ../../../$1/configure

$3: build/ft-env-%.rc build/ft-tmp-%/$1/Makefile
	. $$< \
		&& cd build/ft-tmp-$$*/$1 \
		&& export PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums install
	@touch $$@
endef

define make-git-module-rules
build/.$1-stamp:
	$(RM) -r $1
	git clone $(repo_base_url)/$1$(repo_suffix)
	@mkdir -p $$(@D)
	@touch $$@

$1/configure: build/ft-env-$(build_platform_arch).rc build/.$1-stamp
	. $$< \
		&& cd $$(@D) \
		&& toolchain=$$(shell pwd)/build/ft-toolchain-$$(build_platform_arch) \
		&& export ACLOCAL_FLAGS="-I $$$$toolchain/share/aclocal" \
		&& export ACLOCAL="aclocal -I $$$$toolchain/share/aclocal" \
		&& [ -f autogen.sh ] && NOCONFIGURE=1 ./autogen.sh || autoreconf -ifv

build/ft-tmp-%/$1/Makefile: build/ft-env-%.rc $1/configure $3
	$(RM) -r $$(@D)
	mkdir -p $$(@D)
	. $$< && cd $$(@D) && PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH ../../../$1/configure

$2: build/ft-env-%.rc build/ft-tmp-%/$1/Makefile
	. $$< \
		&& cd build/ft-tmp-$$*/$1 \
		&& export PATH=$$(shell pwd)/build/ft-$$*/bin:$$$$PATH \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums \
		&& make $(MAKE_J) GLIB_GENMARSHAL=glib-genmarshal GLIB_MKENUMS=glib-mkenums install
	@touch $$@
endef


$(eval $(call make-tarball-module-rules,m4,http://gnuftp.uib.no/m4/m4-$(m4_version).tar.gz,build/ft-%/bin/m4,))

$(eval $(call make-tarball-module-rules,autoconf,http://gnuftp.uib.no/autoconf/autoconf-$(autoconf_version).tar.gz,build/ft-%/bin/autoconf,build/ft-%/bin/m4))

$(eval $(call make-tarball-module-rules,automake,http://gnuftp.uib.no/automake/automake-$(automake_version).tar.gz,build/ft-%/bin/automake,build/ft-%/bin/autoconf))
build/ft-tmp-%/.automake-symlink-stamp: build/ft-%/bin/automake
	cd build/ft-$*/bin \
		&& $(RM) aclocal automake \
		&& ln -s aclocal-$(automake_version) aclocal \
		&& ln -s automake-$(automake_version) automake
	@mkdir -p $(@D)
	@touch $@

$(eval $(call make-tarball-module-rules,libtool,http://gnuftp.uib.no/libtool/libtool-$(libtool_version).tar.gz,build/ft-%/bin/libtool,build/ft-tmp-%/.automake-symlink-stamp))

$(eval $(call make-git-module-rules,libffi,build/ft-%/lib/pkgconfig/libffi.pc,build/ft-%/bin/libtool))

$(eval $(call make-git-module-rules,glib,build/ft-%/bin/glib-genmarshal,build/ft-%/lib/pkgconfig/libffi.pc))

$(eval $(call make-tarball-module-rules,pkg-config,http://pkgconfig.freedesktop.org/releases/pkg-config-$(pkg_config_version).tar.gz,build/ft-%/bin/pkg-config,build/ft-%/bin/glib-genmarshal))

$(eval $(call make-git-module-rules,vala,build/ft-%/bin/valac,build/ft-%/bin/glib-genmarshal))


build/ft-env-%.rc:
	FRIDA_ENV_NAME=ft FRIDA_ENV_SDK=none FRIDA_HOST=$* ./releng/setup-env.sh


.PHONY: all
.SECONDARY:
