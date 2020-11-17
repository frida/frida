frida_toolchain_version = 20201106
frida_sdk_version = 20201106
frida_bootstrap_version = 20201028


frida_base_url = https://github.com/frida
gnu_mirror = saimei.ftp.acc.umu.se/mirror/gnu.org/gnu


include releng/system.mk

ifdef FRIDA_HOST
	host_os := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
	host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
else
	host_os := $(build_os)
	host_arch := $(build_arch)
endif
host_os_arch := $(host_os)-$(host_arch)


libiconv_target = lib/libiconv.a
libiconv_version = 1.16
libiconv_url = https://$(gnu_mirror)/libiconv/libiconv-$(libiconv_version).tar.gz
libiconv_hash = e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04
libiconv_patches = \
	$(NULL)
libiconv_options = \
	$(NULL)
libiconv_deps = \
	$(NULL)

m4_target = bin/m4
m4_version = 1.4.18
m4_url = https://$(gnu_mirror)/m4/m4-$(m4_version).tar.gz
m4_hash = ab2633921a5cd38e48797bf5521ad259bdc4b979078034a3b790d7fec5493fab
m4_patches = \
	m4-vasnprintf-apple-fix.patch \
	m4-ftbfs-fix.patch \
	$(NULL)
m4_options = \
	$(NULL)
m4_deps = \
	$(NULL)

autoconf_target = bin/autoconf
autoconf_version = 2.69
autoconf_url = https://$(gnu_mirror)/autoconf/autoconf-$(autoconf_version).tar.gz
autoconf_hash = 954bd69b391edc12d6a4a51a2dd1476543da5c6bbf05a95b59dc0dd6fd4c2969
autoconf_patches = \
	autoconf-uclibc.patch \
	$(NULL)
autoconf_options = \
	$(NULL)
autoconf_deps = \
	m4 \
	$(NULL)

automake_target = bin/automake
automake_version = 1.16.2
automake_url = https://$(gnu_mirror)/automake/automake-$(automake_version).tar.gz
automake_hash = b2f361094b410b4acbf4efba7337bdb786335ca09eb2518635a09fb7319ca5c1
automake_patches = \
	$(NULL)
automake_options = \
	$(NULL)
automake_deps = \
	autoconf \
	$(NULL)
automake_api_version = 1.16

libtool_target = bin/libtool
libtool_version = 2.4.6
libtool_url = https://$(gnu_mirror)/libtool/libtool-$(libtool_version).tar.gz
libtool_hash = e3bd4d5d3d025a36c21dd6af7ea818a2afcd4dfc1ea5a17b39d7854bcd0c06e3
libtool_patches = \
	libtool-fixes.patch \
	$(NULL)
libtool_options = \
	$(NULL)
libtool_deps = \
	automake \
	$(NULL)

gettext_target = bin/autopoint
gettext_version = 0.21
gettext_url = https://$(gnu_mirror)/gettext/gettext-$(gettext_version).tar.gz
gettext_hash = c77d0da3102aec9c07f43671e60611ebff89a996ef159497ce8e59d075786b12
gettext_patches = \
	gettext-static-only.patch \
	gettext-gnulib-glibc.patch \
	$(NULL)
gettext_options = \
	--disable-curses \
	$(NULL)
gettext_deps = \
	libtool \
	$(NULL)

zlib_target = lib/pkgconfig/zlib.pc
zlib_version = 91920caec2160ffd919fd48dc4e7a0f6c3fb36d2
zlib_url = $(frida_base_url)/zlib.git
zlib_hash = $(NULL)
zlib_patches = \
	$(NULL)
zlib_options = \
	$(NULL)
zlib_deps = \
	$(NULL)

libffi_target = lib/pkgconfig/libffi.pc
libffi_version = 4612f7f4b8cfda9a1f07e66d033bb9319860af9b
libffi_url = $(frida_base_url)/libffi.git
libffi_hash = $(NULL)
libffi_patches = \
	$(NULL)
libffi_options = \
	$(NULL)
libffi_deps = \
	$(NULL)

glib_target = lib/pkgconfig/glib-2.0.pc
glib_version = 2930430c4ed9bf7373f1027d6fc41fd54a93b51b
glib_url = $(frida_base_url)/glib.git
glib_hash = $(NULL)
glib_patches = \
	$(NULL)
glib_deps = \
	zlib \
	libffi \
	$(NULL)
glib_options = \
	-Dselinux=disabled \
	-Dxattr=false \
	-Dlibmount=disabled \
	-Dinternal_pcre=true \
	-Dtests=false \
	$(NULL)
ifeq ($(host_os), $(filter $(host_os),macos ios android qnx))
	glib_deps += libiconv
	glib_options += -Diconv=external
endif
ifeq ($(FRIDA_LIBC), uclibc)
	glib_deps += libiconv
	glib_options += -Diconv=external
endif

pkg_config_target = bin/pkg-config
pkg_config_version = b7fb5edc1f1a4fb17cd5cb94f4cf21912184da43
pkg_config_url = $(frida_base_url)/pkg-config.git
pkg_config_hash = $(NULL)
pkg_config_patches = \
	$(NULL)
pkg_config_options = \
	$(NULL)
pkg_config_deps = \
	glib \
	$(NULL)

flex_target = bin/flex
flex_version = 2.6.4
flex_url = https://github.com/westes/flex/releases/download/v$(flex_version)/flex-$(flex_version).tar.gz
flex_hash = e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995
flex_patches = \
	flex-modern-glibc.patch \
	$(NULL)
flex_options = \
	$(NULL)
flex_deps = \
	m4 \
	$(NULL)

bison_target = bin/bison
bison_version = 3.7.3
bison_url = https://$(gnu_mirror)/bison/bison-$(bison_version).tar.gz
bison_hash = 104fe912f2212ab4e4a59df888a93b719a046ffc38d178e943f6c54b1f27b3c7
bison_patches = \
	$(NULL)
bison_options = \
	$(NULL)
bison_deps = \
	m4 \
	$(NULL)

vala_target = bin/valac
vala_version = 5067d99e7b9b8ab1c9393f70596fc5bd4f8b46a2
vala_url = $(frida_base_url)/vala.git
vala_hash = $(NULL)
vala_patches = \
	$(NULL)
vala_options = \
	$(NULL)
vala_deps = \
	glib \
	flex \
	bison \
	$(NULL)

elfutils_target = lib/libelf.a
elfutils_version = b503c358dde835d8a1ae3ebd4968755ff396f814
elfutils_url = git://sourceware.org/git/elfutils.git
elfutils_hash = $(NULL)
elfutils_patches = \
	elfutils-clang.patch \
	elfutils-android.patch \
	elfutils-glibc-compatibility.patch \
	elfutils-musl.patch \
	$(NULL)
elfutils_options = \
	--enable-maintainer-mode \
	--disable-libdebuginfod \
	--disable-debuginfod \
	$(NULL)
elfutils_deps = \
	xz \
	zlib \
	$(NULL)

libdwarf_target = lib/libdwarf.a
libdwarf_version = 20201020
libdwarf_url = https://www.prevanders.net/libdwarf-$(libdwarf_version).tar.gz
libdwarf_hash = 1c5ce59e314c6fe74a1f1b4e2fa12caea9c24429309aa0ebdfa882f74f016eff
libdwarf_patches = \
	$(NULL)
libdwarf_options = \
	$(NULL)
libdwarf_deps = \
	elfutils \
	$(NULL)

xz_target = lib/pkgconfig/liblzma.pc
xz_version = 6c84113065f603803683d30342207c73465bbc12
xz_url = $(frida_base_url)/xz.git
xz_hash = $(NULL)
xz_patches = \
	$(NULL)
xz_options = \
	$(NULL)
xz_deps = \
	$(NULL)

sqlite_target = lib/pkgconfig/sqlite3.pc
sqlite_version = 9f21a054d5c24c2036e9d1b28c630ecda5ae24c3
sqlite_url = $(frida_base_url)/sqlite.git
sqlite_hash = $(NULL)
sqlite_patches = \
	$(NULL)
sqlite_options = \
	$(NULL)
sqlite_deps = \
	$(NULL)

libunwind_target = lib/pkgconfig/libunwind.pc
libunwind_version = 66ca44cd82389cd7cfbbd482e58324a79f6679ab
libunwind_url = $(frida_base_url)/libunwind.git
libunwind_hash = $(NULL)
libunwind_patches = \
	$(NULL)
libunwind_options = \
	--disable-coredump \
	--disable-ptrace \
	--disable-setjmp \
	--disable-debug \
	--disable-msabi-support \
	--enable-minidebuginfo \
	--enable-zlibdebuginfo \
	$(NULL)
libunwind_deps = \
	zlib \
	xz \
	$(NULL)

glib_networking_target = lib/pkgconfig/gioopenssl.pc
glib_networking_version = 85e5a1430517a4682fa4f415b720a870bb35276c
glib_networking_url = $(frida_base_url)/glib-networking.git
glib_networking_hash = $(NULL)
glib_networking_patches = \
	$(NULL)
glib_networking_options = \
	-Dgnutls=disabled \
	-Dopenssl=enabled \
	-Dlibproxy=disabled \
	-Dgnome_proxy=disabled \
	-Dstatic_modules=true \
	$(NULL)
glib_networking_deps = \
	glib \
	openssl \
	$(NULL)

libgee_target = lib/pkgconfig/gee-0.8.pc
libgee_version = c7e96ac037610cc3d0e11dc964b7b1fca479fc2a
libgee_url = $(frida_base_url)/libgee.git
libgee_hash = $(NULL)
libgee_patches = \
	$(NULL)
libgee_options = \
	$(NULL)
libgee_deps = \
	glib \
	$(NULL)

json_glib_target = lib/pkgconfig/json-glib-1.0.pc
json_glib_version = 9dd3b3898a2c41a1f9af24da8bab22e61526d299
json_glib_url = $(frida_base_url)/json-glib.git
json_glib_hash = $(NULL)
json_glib_patches = \
	$(NULL)
json_glib_options = \
	-Dintrospection=disabled \
	-Dtests=false \
	$(NULL)
json_glib_deps = \
	glib \
	$(NULL)

libpsl_target = lib/pkgconfig/libpsl.pc
libpsl_version = 3caf6c33029b6c43fc31ce172badf976f6c37bc4
libpsl_url = $(frida_base_url)/libpsl.git
libpsl_hash = $(NULL)
libpsl_patches = \
	$(NULL)
libpsl_options = \
	-Dtests=false \
	$(NULL)
libpsl_deps = \
	$(NULL)

libxml2_target = lib/pkgconfig/libxml-2.0.pc
libxml2_version = f1845f6fd1c0b6aac0f573c77a8250f8d4eb31fd
libxml2_url = $(frida_base_url)/libxml2.git
libxml2_hash = $(NULL)
libxml2_patches = \
	$(NULL)
libxml2_options = \
	$(NULL)
libxml2_deps = \
	zlib \
	xz \
	$(NULL)

libsoup_target = lib/pkgconfig/libsoup-2.4.pc
libsoup_version = fecd985fa710fa494f62b7bfc9d0728185db2798
libsoup_url = $(frida_base_url)/libsoup.git
libsoup_hash = $(NULL)
libsoup_patches = \
	$(NULL)
libsoup_options = \
	-Dgssapi=disabled \
	-Dtls_check=false \
	-Dgnome=false \
	-Dintrospection=disabled \
	-Dvapi=disabled \
	-Dtests=false \
	-Dsysprof=disabled \
	$(NULL)
libsoup_deps = \
	glib \
	sqlite \
	libpsl \
	libxml2 \
	$(NULL)

capstone_target = lib/pkgconfig/capstone.pc
capstone_version = 03295d19d2c3b0162118a6d9742312301cde1d00
capstone_url = $(frida_base_url)/capstone.git
capstone_hash = $(NULL)
capstone_patches = \
	$(NULL)
capstone_options = \
	-Darchs=$(shell echo $(host_arch) | sed -r \
			-e 's,^x86_64$$,x86,' \
			-e 's,^arm[^0-9].+,arm,' \
			-e 's,^arm64e$$,arm64,' \
			-e 's,^mips.*,mips,' \
		) \
	-Dx86_att_disable=true \
	-Dcli=disabled \
	$(NULL)
capstone_deps = \
	$(NULL)

quickjs_target = lib/pkgconfig/quickjs.pc
quickjs_version = 26ce42ea32a3318b1c6318d4db6cf01ade54be61
quickjs_url = $(frida_base_url)/quickjs.git
quickjs_hash = $(NULL)
quickjs_patches = \
	$(NULL)
quickjs_options = \
	-Dlibc=false \
	-Dbignum=true \
	-Datomics=disabled \
	-Dstack_check=disabled \
	$(NULL)
quickjs_deps = \
	$(NULL)

tinycc_target = lib/pkgconfig/libtcc.pc
tinycc_version = 71c2e2c3d5009bf4ec0c242715250fdc772d49d1
tinycc_url = $(frida_base_url)/tinycc.git
tinycc_hash = $(NULL)
tinycc_patches = \
	$(NULL)
tinycc_options = \
	$(NULL)
tinycc_deps = \
	$(NULL)

openssl_target = lib/pkgconfig/openssl.pc
openssl_version = 1.1.1h
openssl_url = https://www.openssl.org/source/openssl-$(openssl_version).tar.gz
openssl_hash = 5c9ca8774bd7b03e5784f26ae9e9e6d749c9da2438545077e6b3d755a06595d9
openssl_patches = \
	$(NULL)
openssl_options = \
	--openssldir=/etc/ssl \
	no-engine \
	no-tests \
	no-comp \
	no-ssl3 \
	no-zlib \
	no-async \
	no-shared \
	enable-cms \
	$(NULL)
openssl_deps = \
	$(NULL)

v8_target = lib/pkgconfig/v8-$(v8_api_version).pc
v8_version = 9c2e0e6653a4d8a1a7fd2d4109a0b301e81ab7fb
v8_url = $(frida_base_url)/v8.git
v8_hash = $(NULL)
v8_patches = \
	$(NULL)
v8_options = \
	use_thin_lto=false \
	v8_monolithic=true \
	v8_use_external_startup_data=false \
	is_component_build=false \
	v8_enable_debugging_features=false \
	v8_enable_disassembler=false \
	v8_enable_gdbjit=false \
	v8_enable_i18n_support=false \
	v8_untrusted_code_mitigations=false \
	treat_warnings_as_errors=false \
	fatal_linker_warnings=false \
	use_glib=false \
	use_goma=false \
	v8_embedder_string="-frida" \
	$(NULL)
v8_deps = \
	$(NULL)
v8_api_version = 8.0

gn_target = bin/gn
gn_version = 75194c124f158d7fabdc94048f1a3f850a5f0701
gn_url = $(frida_base_url)/gn.git
gn_hash = $(NULL)
gn_patches = \
	$(NULL)
gn_options = \
	$(NULL)
gn_deps = \
	$(NULL)

depot_tools_target = $(NULL)
depot_tools_version = b674f8a27725216bd2201652636649d83064ca4a
depot_tools_url = https://chromium.googlesource.com/chromium/tools/depot_tools.git
depot_tools_hash = $(NULL)
depot_tools_patches = \
	$(NULL)
depot_tools_options = \
	$(NULL)
depot_tools_deps = \
	$(NULL)


define make-base-package-rules

.PHONY: $1

$1: build/$3-$2/manifest/$1.pkg

deps/.$1-stamp:
	$$(call grab-and-prepare,$1)
	@touch $$@

endef


define make-base-clean-commands
	$(RM) build/$2-$3/manifest/$1.pkg
	$(RM) build/$2-$3/$$($$(subst -,_,$1)_target)
	$(RM) -r build/$2-tmp-$3/$1
endef


define make-base-distclean-commands
	$(RM) deps/.$1-stamp
	$(RM) -r deps/$1
endef


define make-meson-package-rules-for-env

$(call make-base-package-rules,$1,$(host_os_arch),$2)

build/$2-tmp-%/$1/build.ninja: build/$2-env-%.rc deps/.$1-stamp \
		$$(foreach dep,$$($$(subst -,_,$1)_deps),build/$2-%/manifest/$$(dep).pkg) \
		releng/meson/meson.py
	@$(call print-status,$1,Configuring)
	@$(RM) -r $$(@D)
	@mkdir -p $$(@D)
	@(set -x \
		&& . build/$2-meson-env-$$*.rc \
		&& . build/$2-config-$$*.site \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& $(MESON) \
			--cross-file build/$2-$$*.txt \
			--prefix $$$$frida_prefix \
			--libdir $$$$frida_prefix/lib \
			--default-library static \
			$$(FRIDA_MESONFLAGS_BOTTLE) \
			$$($$(subst -,_,$1)_options) \
			$$(@D) \
			deps/$1 \
	) >$$(@D)/build.log 2>&1

build/$2-%/$$($$(subst -,_,$1)_target): build/$2-env-%.rc build/$2-tmp-%/$1/build.ninja
	@$(call print-status,$1,Building)
	@(set -x \
		&& . $$< \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& $(NINJA) -C build/$2-tmp-$$*/$1 install \
	) >>build/$2-tmp-$$*/$1/build.log 2>&1
	@touch $$@

build/$2-%/manifest/$1.pkg: build/$2-%/$$($$(subst -,_,$1)_target)
	@mkdir -p $$(@D)
	@cd build/$2-tmp-$$*/$1 && $(MESON) introspect --installed --indent \
		| grep ": " \
		| cut -f4 -d'"' \
		| cut -c$$(strip $$(shell echo $$(abspath build/$2-$$*x) | wc -c))- \
		> $$(abspath $$@)

.PHONY: clean-$1 distclean-$1

clean-$1:
	if [ -f build/$2-tmp-$(host_os_arch)/$1/build.ninja ]; then \
		. build/$2-env-$(host_os_arch).rc; \
		$(NINJA) -C build/$2-tmp-$(host_os_arch)/$1 uninstall &>/dev/null; \
	fi
	$(call make-base-clean-commands,$1,$2,$(host_os_arch))

distclean-$1: clean-$1
	$(call make-base-distclean-commands,$1)

endef


define make-autotools-base-package-rules-for-env

$(call make-base-package-rules,$1,$(host_os_arch),$2)

deps/$1/configure: build/$2-env-$(build_os_arch).rc deps/.$1-stamp \
		$$(foreach dep,$$($$(subst -,_,$1)_deps),build/$2-%/manifest/$$(dep).pkg)
	@. $$< \
		&& cd $$(@D) \
		&& if [ ! -f configure ] || [ -f ../.$1-reconf-needed ]; then \
			$(call print-status,$1,Generating build system); \
			autoreconf -if &>/dev/null || exit 1; \
			rm -f ../.$1-reconf-needed; \
		fi
	@touch $$@

build/$2-tmp-%/$1/Makefile: build/$2-env-%.rc deps/$1/configure
	@$(call print-status,$1,Configuring)
	@$(RM) -r $$(@D)
	@mkdir -p $$(@D)
	@(set -x \
		&& . $$< \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& cd $$(@D) \
		&& ../../../deps/$1/configure $$($$(subst -,_,$1)_options) \
	) >$$(@D)/build.log 2>&1

endef


define make-autotools-package-rules-for-env

$(call make-autotools-base-package-rules-for-env,$1,$2)

.PHONY: clean-$1 distclean-$1

clean-$1:
	[ -f build/$2-tmp-$(host_os_arch)/$1/Makefile ] \
		&& $(MAKE) -C build/$2-tmp-$(host_os_arch)/$1 uninstall &>/dev/null || true
	$(call make-base-clean-commands,$1,$2,$(host_os_arch))

distclean-$1: clean-$1
	$(call make-base-distclean-commands,$1)

build/$2-%/$($(subst -,_,$1)_target): build/$2-env-%.rc build/$2-tmp-%/$1/Makefile
	@$(call print-status,$1,Building)
	@(set -x \
		&& . $$< \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& cd build/$2-tmp-$$*/$1 \
		&& $(MAKE) $$(MAKE_J) \
		&& $(MAKE) $$(MAKE_J) install \
	) >>build/$2-tmp-$$*/$1/build.log 2>&1
	@touch $$@

$(call make-autotools-manifest-rule,$1,$2)

endef


define make-autotools-manifest-rule

build/$2-%/manifest/$1.pkg: build/$2-%/$$($$(subst -,_,$1)_target)
	@mkdir -p $$(@D)
	@cd build/$2-tmp-$$*/$1 \
		&& $(RM) -r __pkg__ \
		&& mkdir __pkg__ \
		&& $(MAKE) $$(MAKE_J) install DESTDIR=$$$$(pwd)/__pkg__ &>/dev/null \
		&& cd __pkg__ \
		&& find . -type f \
			| cut -c$$(strip $$(shell echo $$(abspath build/$2-$$*xx) | wc -c))- \
			> $$(abspath $$@) \
		&& $(RM) -r __pkg__

endef


define grab-and-prepare
	$(if $($(subst -,_,$1)_hash),
		$(call grab-and-prepare-tarball,$1),
		$(call grab-and-prepare-repo,$1))
endef


define grab-and-prepare-tarball
	@$(RM) -r deps/$1
	@mkdir -p deps/$1

	@url=$($(subst -,_,$1)_url) \
		&& version=$($(subst -,_,$1)_version) \
		&& expected_hash=$($(subst -,_,$1)_hash) \
		&& $(call print-tarball-banner,$1,$$version,$$url,$$expected_hash) \
		&& $(call print-status,$1,Downloading) \
		&& if command -v curl >/dev/null; then \
			curl -sSfLo deps/.$1-tarball $$url; \
		else \
			wget -qO deps/.$1-tarball $$url; \
		fi \
		&& $(call print-status,$1,Verifying) \
		&& actual_hash=$$(shasum -a 256 -b deps/.$1-tarball | awk '{ print $$1; }') \
		&& case $$actual_hash in \
			$$expected_hash) \
				;; \
			*) \
				echo "$1 tarball is corrupted; its hash is: $$actual_hash"; \
				exit 1; \
				;; \
		esac

	@$(call print-status,$1,Extracting to deps/$1)
	@tar -C deps/$1 -x -f deps/.$1-tarball -z --strip-components 1

	$(call apply-patches,$1)

	@rm deps/.$1-tarball
endef


define grab-and-prepare-repo
	@$(RM) -r deps/$1

	@url=$($(subst -,_,$1)_url) \
		&& version=$($(subst -,_,$1)_version) \
		&& $(call print-repo-banner,$1,$$version,$$url) \
		&& $(call print-status,$1,Cloning into deps/$1) \
		&& git clone -q --recurse-submodules $$url deps/$1 \
		&& cd deps/$1 \
		&& git checkout -q $$version

	$(call apply-patches,$1)
endef


define apply-patches
	@cd deps/$1 \
		&& for patch in $($(subst -,_,$1)_patches); do \
			file=../../releng/patches/$$patch; \
			\
			$(call print-status,$1,Applying $$patch); \
			patch -p1 < $$file &>/dev/null || exit 1; \
			\
			if grep '+++' $$file | grep -Eq 'configure\.ac|Makefile\.am'; then \
				touch ../.$1-reconf-needed; \
			fi \
		done
endef


define print-status
	echo -e "│ \\033[1m$1\\033[0m :: $2"
endef


define print-tarball-banner
	echo -e "╭────\n│ 📦 \\033[1m$1\\033[0m $2\n├───────\n│ URL: $3\n│ SHA: $4\n├────────────"
endef


define print-repo-banner
	echo -e "╭────\n│ 📦 \\033[1m$1\\033[0m\n├───────────────────────────────────────────────╮\n│ URL: $3\n│ CID: $2\n├───────────────────────────────────────────────╯"
endef
