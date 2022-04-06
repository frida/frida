frida_deps_version = 20220318
frida_bootstrap_version = 20220130


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


ninja_name = Ninja
ninja_version = v1.10.2
ninja_url = https://github.com/ninja-build/ninja.git
ninja_hash = $(NULL)
ninja_recipe = custom
ninja_patches = \
	$(NULL)
ninja_options = \
	$(NULL)
ninja_deps = \
	$(NULL)
ninja_deps_for_build = \
	$(NULL)

frida_elf_cleaner_name = frida-elf-cleaner
frida_elf_cleaner_version = 821c6319f5545f092d815233df73fc253ca4c603
frida_elf_cleaner_url = $(frida_base_url)/frida-elf-cleaner.git
frida_elf_cleaner_hash = $(NULL)
frida_elf_cleaner_recipe = meson
frida_elf_cleaner_patches = \
	$(NULL)
frida_elf_cleaner_options = \
	$(NULL)
frida_elf_cleaner_deps = \
	$(NULL)
frida_elf_cleaner_deps_for_build = \
	$(NULL)

libiconv_name = libiconv
libiconv_version = 78d655ecc65b773b8e6642ea4be89e6f51d2c518
libiconv_url = $(frida_base_url)/libiconv.git
libiconv_hash = $(NULL)
libiconv_recipe = meson
libiconv_patches = \
	$(NULL)
libiconv_options = \
	$(NULL)
libiconv_deps = \
	$(NULL)
libiconv_deps_for_build = \
	$(NULL)

zlib_name = zlib
zlib_version = 64dd495da9b75ad61400a8de5b2a1bbb9fbbffbb
zlib_url = $(frida_base_url)/zlib.git
zlib_hash = $(NULL)
zlib_recipe = meson
zlib_patches = \
	$(NULL)
zlib_options = \
	$(NULL)
zlib_deps = \
	$(NULL)
zlib_deps_for_build = \
	$(NULL)

libffi_name = libffi
libffi_version = cf69550af33ea8af7ace9a5f5ca59b67b00fc8dc
libffi_url = $(frida_base_url)/libffi.git
libffi_hash = $(NULL)
libffi_recipe = meson
libffi_patches = \
	$(NULL)
libffi_options = \
	$(NULL)
libffi_deps = \
	$(NULL)
libffi_deps_for_build = \
	$(NULL)

selinux_name = SELinux Userspace
selinux_version = 9c7ba053bb075cace088d268fda400f6bc4ab14c
selinux_url = $(frida_base_url)/selinux.git
selinux_hash = $(NULL)
selinux_recipe = meson
selinux_patches = \
	$(NULL)
selinux_options = \
	-Dregex=disabled \
	$(NULL)
selinux_deps = \
	$(NULL)
selinux_deps_for_build = \
	$(NULL)

glib_name = GLib
glib_version = bb2a8f4626097a6b1af722cdadb2b23135ca666e
glib_url = $(frida_base_url)/glib.git
glib_hash = $(NULL)
glib_recipe = meson
glib_patches = \
	$(NULL)
glib_deps = \
	zlib \
	libffi \
	$(NULL)
glib_deps_for_build = \
	$(NULL)
glib_options = \
	-Dselinux=disabled \
	-Dxattr=false \
	-Dlibmount=disabled \
	-Dtests=false \
	-Dglib_debug=disabled \
	-Dglib_assert=false \
	-Dglib_checks=false \
	--force-fallback-for=pcre \
	$(NULL)
ifeq ($(host_os), $(filter $(host_os),macos ios))
# Use Apple's iconv by default to make our toolchain smaller.
# Our SDK will pull in its own.
glib_options += -Diconv=external
endif
ifeq ($(host_os), $(filter $(host_os),android qnx))
glib_options += -Diconv=external
glib_deps += libiconv
endif
ifeq ($(FRIDA_LIBC), uclibc)
glib_options += -Diconv=external
glib_deps += libiconv
endif

pkg_config_name = pkg-config
pkg_config_version = 4696795673d1d3dec46b663df48f8cbf66461d14
pkg_config_url = $(frida_base_url)/pkg-config.git
pkg_config_hash = $(NULL)
pkg_config_recipe = meson
pkg_config_patches = \
	$(NULL)
pkg_config_options = \
	$(NULL)
pkg_config_deps = \
	glib \
	$(NULL)
pkg_config_deps_for_build = \
	$(NULL)

flex_name = Flex
flex_version = 2.6.4
flex_url = https://github.com/westes/flex/releases/download/v$(flex_version)/flex-$(flex_version).tar.gz
flex_hash = e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995
flex_recipe = autotools
flex_patches = \
	flex-modern-glibc.patch \
	$(NULL)
flex_options = \
	$(NULL)
flex_deps = \
	$(NULL)
flex_deps_for_build = \
	$(NULL)

bison_name = Bison
bison_version = 3.8.2
bison_url = https://$(gnu_mirror)/bison/bison-$(bison_version).tar.gz
bison_hash = 06c9e13bdf7eb24d4ceb6b59205a4f67c2c7e7213119644430fe82fbd14a0abb
bison_recipe = autotools
bison_patches = \
	$(NULL)
bison_options = \
	$(NULL)
bison_deps = \
	$(NULL)
bison_deps_for_build = \
	$(NULL)

vala_name = Vala
vala_version = e29e312e8529e59f2967b86b9bbb8584dcde7c30
vala_url = $(frida_base_url)/vala.git
vala_hash = $(NULL)
vala_recipe = meson
vala_patches = \
	$(NULL)
vala_options = \
	$(NULL)
vala_deps = \
	glib \
	$(NULL)
vala_deps_for_build = \
	flex \
	bison \
	$(NULL)

elfutils_name = elfutils
elfutils_version = 1284bbc128473aea220337685985d465607fbac8
elfutils_url = $(frida_base_url)/elfutils.git
elfutils_hash = $(NULL)
elfutils_recipe = meson
elfutils_patches = \
	$(NULL)
elfutils_options = \
	$(NULL)
elfutils_deps = \
	zlib \
	$(NULL)
elfutils_deps_for_build = \
	$(NULL)

libdwarf_name = libdwarf
libdwarf_version = 0a5640598201d9a025c33055dde82d6597fcd650
libdwarf_url = $(frida_base_url)/libdwarf.git
libdwarf_hash = $(NULL)
libdwarf_recipe = meson
libdwarf_patches = \
	$(NULL)
libdwarf_options = \
	$(NULL)
libdwarf_deps = \
	$(NULL)
ifneq ($(host_os), freebsd)
libdwarf_deps += elfutils
endif
libdwarf_deps_for_build = \
	$(NULL)

xz_name = XZ Utils
xz_version = 83617aba90b2254c91a1ebf1da29240c267151c6
xz_url = $(frida_base_url)/xz.git
xz_hash = $(NULL)
xz_recipe = meson
xz_patches = \
	$(NULL)
xz_options = \
	-Dcli=disabled \
	$(NULL)
xz_deps = \
	$(NULL)
xz_deps_for_build = \
	$(NULL)

brotli_name = Brotli
brotli_version = 8abf3188d1ef4bb8a633f894fec731bdd510ee49
brotli_url = $(frida_base_url)/brotli.git
brotli_hash = $(NULL)
brotli_recipe = meson
brotli_patches = \
	$(NULL)
brotli_options = \
	$(NULL)
brotli_deps = \
	$(NULL)
brotli_deps_for_build = \
	$(NULL)

minizip_name = minizip-ng
minizip_version = 535c1b9150e5e47b9a533b6f16787042da92ac63
minizip_url = $(frida_base_url)/minizip-ng.git
minizip_hash = $(NULL)
minizip_recipe = meson
minizip_patches = \
	$(NULL)
minizip_options = \
	-Dzlib=enabled \
	-Dlzma=disabled \
	$(NULL)
minizip_deps = \
	zlib \
	$(NULL)
minizip_deps_for_build = \
	$(NULL)
ifeq ($(host_os), $(filter $(host_os),macos ios android qnx))
minizip_deps += libiconv
endif
ifeq ($(FRIDA_LIBC), uclibc)
minizip_deps += libiconv
endif

sqlite_name = SQLite
sqlite_version = 6b876d7c22f10488477d106dfe51f3fbd4ce2d20
sqlite_url = $(frida_base_url)/sqlite.git
sqlite_hash = $(NULL)
sqlite_recipe = meson
sqlite_patches = \
	$(NULL)
sqlite_options = \
	$(NULL)
sqlite_deps = \
	$(NULL)
sqlite_deps_for_build = \
	$(NULL)

libunwind_name = libunwind
libunwind_version = 12ac8fe53a2cb23501116a83ee59bd57da06bfe9
libunwind_url = $(frida_base_url)/libunwind.git
libunwind_hash = $(NULL)
libunwind_recipe = meson
libunwind_patches = \
	$(NULL)
libunwind_options = \
	-Dgeneric_library=disabled \
	-Dcoredump_library=disabled \
	-Dptrace_library=disabled \
	-Dsetjmp_library=disabled \
	-Dmsabi_support=false \
	-Dminidebuginfo=enabled \
	-Dzlibdebuginfo=enabled \
	$(NULL)
libunwind_deps = \
	zlib \
	xz \
	$(NULL)
libunwind_deps_for_build = \
	$(NULL)

glib_networking_name = glib-networking
glib_networking_version = 05d01a444f1738b3cfe2d583f49fdba1357e3184
glib_networking_url = $(frida_base_url)/glib-networking.git
glib_networking_hash = $(NULL)
glib_networking_recipe = meson
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
glib_networking_deps_for_build = \
	$(NULL)

libnice_name = libnice
libnice_version = f9bf93471ab128821ceebf6bf3e4aa3e941af4b0
libnice_url = $(frida_base_url)/libnice.git
libnice_hash = $(NULL)
libnice_recipe = meson
libnice_patches = \
	$(NULL)
libnice_options = \
	-Dgupnp=disabled \
	-Dgstreamer=disabled \
	-Dcrypto-library=openssl \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dintrospection=disabled \
	$(NULL)
libnice_deps = \
	glib \
	openssl \
	$(NULL)
libnice_deps_for_build = \
	$(NULL)

usrsctp_name = usrsctp
usrsctp_version = f099f66679921ac6bc0568320cf6ea3b861c4085
usrsctp_url = $(frida_base_url)/usrsctp.git
usrsctp_hash = $(NULL)
usrsctp_recipe = meson
usrsctp_patches = \
	$(NULL)
usrsctp_options = \
	-Dsctp_inet=false \
	-Dsctp_inet6=false \
	-Dsctp_build_programs=false \
	$(NULL)
usrsctp_deps = \
	$(NULL)
usrsctp_deps_for_build = \
	$(NULL)

libgee_name = libgee
libgee_version = 5b00fd64096b369a04fe04a7246e1927c3fedbd7
libgee_url = $(frida_base_url)/libgee.git
libgee_hash = $(NULL)
libgee_recipe = meson
libgee_patches = \
	$(NULL)
libgee_options = \
	$(NULL)
libgee_deps = \
	glib \
	$(NULL)
libgee_deps_for_build = \
	$(NULL)

json_glib_name = JSON-GLib
json_glib_version = a8d3ac569bfaf509e2a20b55ee4fd6b89851b8b1
json_glib_url = $(frida_base_url)/json-glib.git
json_glib_hash = $(NULL)
json_glib_recipe = meson
json_glib_patches = \
	$(NULL)
json_glib_options = \
	-Dintrospection=disabled \
	-Dgtk_doc=disabled \
	-Dtests=false \
	$(NULL)
json_glib_deps = \
	glib \
	$(NULL)
json_glib_deps_for_build = \
	$(NULL)

libpsl_name = libpsl
libpsl_version = dc7fce073dfb66f055ce91ebeff41f60b9db2ce4
libpsl_url = $(frida_base_url)/libpsl.git
libpsl_hash = $(NULL)
libpsl_recipe = meson
libpsl_patches = \
	$(NULL)
libpsl_options = \
	-Druntime=no \
	-Dbuiltin=no \
	-Dtests=false \
	$(NULL)
libpsl_deps = \
	$(NULL)
libpsl_deps_for_build = \
	$(NULL)

libxml2_name = libxml2
libxml2_version = 769bc59b47daa8172bb57255ed9a4987937878d2
libxml2_url = $(frida_base_url)/libxml2.git
libxml2_hash = $(NULL)
libxml2_recipe = meson
libxml2_patches = \
	$(NULL)
libxml2_options = \
	$(NULL)
libxml2_deps = \
	zlib \
	xz \
	$(NULL)
libxml2_deps_for_build = \
	$(NULL)

libsoup_name = libsoup
libsoup_version = f8683845a91d165aaaefa7db5cf3afbf95f06a60
libsoup_url = $(frida_base_url)/libsoup.git
libsoup_hash = $(NULL)
libsoup_recipe = meson
libsoup_patches = \
	$(NULL)
libsoup_options = \
	-Dgssapi=disabled \
	-Dntlm=disabled \
	-Dbrotli=enabled \
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
	brotli \
	$(NULL)
libsoup_deps_for_build = \
	$(NULL)

capstone_name = Capstone
capstone_version = 6182ac33e0e5876bdf39f7e60416ce9fd73ce61a
capstone_url = $(frida_base_url)/capstone.git
capstone_hash = $(NULL)
capstone_recipe = meson
capstone_patches = \
	$(NULL)
capstone_options = \
	-Darchs=$(capstone_archs) \
	-Dx86_att_disable=true \
	-Dcli=disabled \
	$(NULL)
capstone_deps = \
	$(NULL)
capstone_deps_for_build = \
	$(NULL)
capstone_archs := $(shell echo $(host_arch) | sed $(sed_regex_option) \
		-e 's,^x86_64$$,x86,' \
		-e 's,^arm[^0-9].+,arm,' \
		-e 's,^arm64e$$,arm64,' \
		-e 's,^arm64eoabi$$,arm64,' \
		-e 's,^mips.*,mips,' \
		-e 's,^s390x$$,sysz,' \
	)

quickjs_name = QuickJS
quickjs_version = 1a2ede336e321c2a7769dba8734bb3fefa7a047a
quickjs_url = $(frida_base_url)/quickjs.git
quickjs_hash = $(NULL)
quickjs_recipe = meson
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
quickjs_deps_for_build = \
	$(NULL)

tinycc_name = TinyCC
tinycc_version = a438164dd4c453ae62c1224b4b7997507a388b3d
tinycc_url = $(frida_base_url)/tinycc.git
tinycc_hash = $(NULL)
tinycc_recipe = meson
tinycc_patches = \
	$(NULL)
tinycc_options = \
	$(NULL)
tinycc_deps = \
	$(NULL)
tinycc_deps_for_build = \
	$(NULL)

openssl_name = OpenSSL
openssl_version = cf2e4e9b324c6e11d6661b8bce6d0d6aa6afd3a5
openssl_url = $(frida_base_url)/openssl.git
openssl_hash = $(NULL)
openssl_recipe = meson
openssl_patches = \
	$(NULL)
openssl_options = \
	$(NULL)
openssl_deps = \
	$(NULL)
openssl_deps_for_build = \
	$(NULL)

v8_name = V8
v8_version = c2f50af5b45285e978d711f4750c64767b190040
v8_url = $(frida_base_url)/v8.git
v8_hash = $(NULL)
v8_recipe = custom
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
v8_deps_for_build = \
	$(NULL)
v8_api_version = 8.0

gn_name = GN
gn_version = dd3501bfb77bafc41e7493c92e2684fa9709770b
gn_url = $(frida_base_url)/gn.git
gn_hash = $(NULL)
gn_recipe = custom
gn_patches = \
	$(NULL)
gn_options = \
	$(NULL)
gn_deps = \
	$(NULL)
gn_deps_for_build = \
	$(NULL)

depot_tools_name = depot_tools
depot_tools_version = cb340f5b7bbdcaba0fad346b08db91538619a531
depot_tools_url = https://chromium.googlesource.com/chromium/tools/depot_tools.git
depot_tools_hash = $(NULL)
depot_tools_recipe = custom
depot_tools_patches = \
	depot_tools-os-support.patch \
	$(NULL)
depot_tools_options = \
	$(NULL)
depot_tools_deps = \
	$(NULL)
depot_tools_deps_for_build = \
	$(NULL)


define expand-packages
$(sort $(foreach pkg, $1, $(pkg) $($(subst -,_,$(pkg))_deps) $($(subst -,_,$(pkg))_deps_for_build)))
endef


define make-package-rules

$(foreach pkg, $(call expand-packages,$1), \
	$(if $(findstring meson,$($(subst -,_,$(pkg))_recipe)), $(call make-meson-package-rules,$(pkg),$2), \
	$(if $(findstring autotools,$($(subst -,_,$(pkg))_recipe)), $(call make-autotools-package-rules,$(pkg),$2),)))

endef


define make-meson-package-rules

$(call make-base-package-rules,$1,$2,$(host_os_arch))

deps/.$1-stamp:
	$$(call grab-and-prepare,$1)
	@touch $$@

build/$2-%/manifest/$1.pkg: build/$2-env-%.rc deps/.$1-stamp \
		$(foreach dep, $($(subst -,_,$1)_deps), build/$2-%/manifest/$(dep).pkg) \
		$(foreach dep, $($(subst -,_,$1)_deps_for_build), build/$2-$(build_os_arch)/manifest/$(dep).pkg) \
		releng/meson/meson.py
	@$(call print-status,$1,Building)
	@prefix=$$(shell pwd)/build/$2-$$*; \
	builddir=build/$2-tmp-$$*/$1; \
	$(RM) -r $$$$builddir; \
	mkdir -p $$$$builddir; \
	(set -x \
		&& . build/$2-env-$$*.rc \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& $(call print-status,$1,Configuring) \
		&& meson_args="--native-file build/$2-$(build_os_arch).txt" \
		&& if [ $$* != $(build_os_arch) ]; then \
			meson_args="$$$$meson_args --cross-file build/$2-$$*.txt"; \
		fi \
		&& $(MESON) setup $$$$meson_args \
			--prefix "$$$$prefix" \
			--libdir "$$$$prefix/lib" \
			--default-library static \
			$$(FRIDA_FLAGS_BOTTLE) \
			$$($$(subst -,_,$1)_options) \
			$$$$builddir \
			deps/$1 \
		&& $(MESON) install -C $$$$builddir \
	) >$$$$builddir/build.log 2>&1 \
	&& $(call print-status,$1,Generating manifest) \
	&& (set -x \
		&& cd $$$$builddir \
		&& mkdir -p "$$$$prefix/manifest" \
		&& $(MESON) introspect --installed --indent \
			| grep ": " \
			| cut -f4 -d'"' \
			| cut -c$$(strip $$(shell echo $$(shell pwd)/build/$2-$$*x | wc -c))- \
			| sort \
			> "$$$$prefix/manifest/$1.pkg" \
	) >>$$$$builddir/build.log 2>&1 || (echo "failed - see $$$$builddir/build.log for more information"; exit 1) \

endef


define make-autotools-package-rules

$(call make-autotools-package-rules-without-build-rule,$1,$2)

$(call make-autotools-build-rule,$1,$2)

endef


define make-autotools-package-rules-without-build-rule

$(call make-base-package-rules,$1,$2,$(host_os_arch))

deps/.$1-stamp:
	$$(call grab-and-prepare,$1)
	@touch $$@

$(call make-autotools-configure-rule,$1,$2)

endef


define make-autotools-configure-rule

build/$2-tmp-%/$1/Makefile: build/$2-env-%.rc deps/.$1-stamp \
		$(foreach dep, $($(subst -,_,$1)_deps), build/$2-%/manifest/$(dep).pkg) \
		$(foreach dep, $($(subst -,_,$1)_deps_for_build), build/$2-$(build_os_arch)/manifest/$(dep).pkg)
	@$(call print-status,$1,Configuring)
	@$(RM) -r $$(@D)
	@mkdir -p $$(@D)
	@(set -x \
		&& . $$< \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& cd $$(@D) \
		&& ../../../deps/$1/configure \
			--prefix=$$(shell pwd)/build/$2-$$* \
			$$($$(subst -,_,$1)_options) \
	) >$$(@D)/build.log 2>&1 || (echo "failed - see $$(@D)/build.log for more information"; exit 1)

endef


define make-autotools-build-rule

build/$2-%/manifest/$1.pkg: build/$2-env-%.rc build/$2-tmp-%/$1/Makefile
	@$(call print-status,$1,Building)
	@builddir=build/$2-tmp-$$*/$1; \
	(set -x \
		&& . $$< \
		&& export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH" \
		&& cd "$$$$builddir" \
		&& $(MAKE) $(MAKE_J) \
		&& $(MAKE) $(MAKE_J) install \
	) >>$$$$builddir/build.log 2>&1 \
	&& $(call print-status,$1,Generating manifest) \
	&& (set -x; \
		$$(call make-autotools-manifest-commands,$1,$2,$$*,) \
	) >>$$$$builddir/build.log 2>&1 || (echo "failed - see $$$$builddir/build.log for more information"; exit 1)

endef


define make-autotools-manifest-commands
	( \
		prefix=$(shell pwd)/build/$2-$3 \
		&& mkdir -p $$prefix/manifest \
		&& cd build/$2-tmp-$3/$1 \
		&& $(RM) -r __pkg__ \
		&& mkdir __pkg__ \
		&& $(MAKE) $(MAKE_J) $(if $4,$4,install) DESTDIR="$(shell pwd)/build/$2-tmp-$3/$1/__pkg__" &>/dev/null \
		&& cd __pkg__ \
		&& find . -type f \
			| cut -c$(strip $(shell echo $(shell pwd)/build/$2-$3xx | wc -c))- \
			| sort \
			> "$$prefix/manifest/$1.pkg" \
		&& $(RM) -r __pkg__ \
	)
endef


define make-base-package-rules

$(call make-build-incremental-package-rule,$1,$2,$3)

$(call make-clean-package-rules,$1,$2,$3)

$(call make-symlinks-package-rule,$1,$2,$3)

endef


define make-build-incremental-package-rule

.PHONY: $1

$1: build/$2-$3/manifest/$1.pkg
	builddir=build/$2-tmp-$3/$1; \
	export PATH="$$(shell pwd)/build/$2-$(build_os_arch)/bin:$$$$PATH"; \
	if [ -f deps/$1/meson.build ]; then \
		. build/$2-env-$3.rc; \
		$(MESON) install -C $$$$builddir; \
	else \
		echo "Incremental compilation not supported for: $1"; \
	fi

endef


define make-clean-package-rules

.PHONY: clean-$1 distclean-$1

clean-$1:
	@if [ -f build/$2-$3/manifest/$1.pkg ]; then \
		cd build/$2-$3; \
		cat manifest/$1.pkg | while read entry; do \
			echo $(RM) build/$2-$3/$$$$entry; \
			$(RM) $$$$entry; \
			rmdir -p $$$$(dirname $$$$entry) 2>/dev/null || true; \
		done \
	fi
	$(RM) build/$2-$3/manifest/$1.pkg
	$(RM) -r build/$2-tmp-$3/$1

distclean-$1: clean-$1
	$(RM) deps/.$1-stamp
	$(RM) -r deps/$1

endef


define make-symlinks-package-rule

.PHONY: symlinks-$1

symlinks-$1: build/$2-$3/manifest/$1.pkg
	@sdkroot=build/sdk-$$(host_os_arch); \
	if [ -d $$$$sdkroot ]; then \
		cd $$$$sdkroot; \
		if [ -f manifest/$1.pkg ]; then \
			for old_entry in $$$$(cat manifest/$1.pkg); do \
				$(RM) $$$$old_entry; \
			done; \
		fi; \
		for entry in $$$$(cat ../$2-$3/manifest/$1.pkg); do \
			echo "✓ $$$$entry"; \
			$(RM) $$$$entry; \
			mkdir -p $$$$(dirname $$$$entry); \
			original_relpath=$$$$($(PYTHON3) -c "import os.path; import sys; \
				print(os.path.relpath('../$2-$3/$$$$entry', os.path.dirname('$$$$entry')))"); \
			ln -s $$$$original_relpath $$$$entry; \
		done; \
	fi

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
		&& name="$($(subst -,_,$1)_name)" \
		&& version=$($(subst -,_,$1)_version) \
		&& expected_hash=$($(subst -,_,$1)_hash) \
		&& $(call print-tarball-banner,"$$name",$$version,$$url,$$expected_hash) \
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
	@tar -C deps/$1 -x -f deps/.$1-tarball --strip-components 1

	$(call apply-patches,$1)

	@rm deps/.$1-tarball
endef


define grab-and-prepare-repo
	@$(RM) -r deps/$1

	@url=$($(subst -,_,$1)_url) \
		&& name="$($(subst -,_,$1)_name)" \
		&& version=$($(subst -,_,$1)_version) \
		&& $(call print-repo-banner,"$$name",$$version,$$url) \
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
		done
endef


define print-status
	echo -e "│ \\033[1m$1\\033[0m :: $2"
endef


define print-tarball-banner
	echo -e "\n╭────\n│ 📦 \\033[1m$1\\033[0m $2\n├───────\n│ URL: $3\n│ SHA: $4\n├────────────"
endef


define print-repo-banner
	echo -e "\n╭────\n│ 📦 \\033[1m$1\\033[0m\n├───────────────────────────────────────────────╮\n│ URL: $3\n│ CID: $2\n├───────────────────────────────────────────────╯"
endef
