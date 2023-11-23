frida_deps_version = 20231124
frida_bootstrap_version = 20231116


frida_base_url = https://github.com/frida


include releng/system.mk

ifdef FRIDA_HOST
host_os := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
host_variant := $(shell echo $(FRIDA_HOST) | cut -f3 -d"-")
host_machine := $(FRIDA_HOST)
else
host_os := $(build_os)
host_arch := $(build_arch)
host_variant :=
host_machine := $(host_os)-$(host_arch)
endif
host_os_arch := $(host_os)-$(host_arch)


MAKE_J ?= -j 8
SHELL := $(shell which bash)


ninja_name = Ninja
ninja_version = v1.11.1
ninja_url = https://github.com/ninja-build/ninja.git
ninja_recipe = custom
ninja_patches = \
	ninja-linux-arm-ppoll-fallback.patch \
	$(NULL)
ninja_options = \
	$(NULL)
ninja_deps = \
	$(NULL)
ninja_deps_for_build = \
	$(NULL)

termux_elf_cleaner_name = termux-elf-cleaner
termux_elf_cleaner_version = c30d16bc119dae547c51c16e1cab37b08e240f6a
termux_elf_cleaner_url = $(frida_base_url)/termux-elf-cleaner.git
termux_elf_cleaner_recipe = meson
termux_elf_cleaner_patches = \
	$(NULL)
termux_elf_cleaner_options = \
	$(NULL)
termux_elf_cleaner_deps = \
	$(NULL)
termux_elf_cleaner_deps_for_build = \
	$(NULL)

libiconv_name = libiconv
libiconv_version = 9732614f0ee778d58acccd802ffe907a1b0a3e7a
libiconv_url = $(frida_base_url)/libiconv.git
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
zlib_version = a912d314d0812518d4bbd715a981e6c9484b550d
zlib_url = $(frida_base_url)/zlib.git
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
libffi_version = 938b07aa8280bcc4f92f08a0fdbb4ad81024bde0
libffi_url = $(frida_base_url)/libffi.git
libffi_recipe = meson
libffi_patches = \
	$(NULL)
libffi_options = \
	-Dexe_static_tramp=false \
	-Dtests=false \
	$(NULL)
libffi_deps = \
	$(NULL)
libffi_deps_for_build = \
	$(NULL)

pcre2_name = PCRE2
pcre2_version = b47486922fdc3486499b310dc9cf903449700474
pcre2_url = $(frida_base_url)/pcre2.git
pcre2_recipe = meson
pcre2_patches = \
	$(NULL)
pcre2_options = \
	-Dgrep=false \
	-Dtest=false \
	$(NULL)
pcre2_deps = \
	$(NULL)
pcre2_deps_for_build = \
	$(NULL)

selinux_name = SELinux Userspace
selinux_version = 7d03ec4cd2e08e3bb0c2390a6e68beb3978e2816
selinux_url = $(frida_base_url)/selinux.git
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
glib_version = 638b2073275bf1fde72b02bf507ef66c1f79743b
glib_url = $(frida_base_url)/glib.git
glib_recipe = meson
glib_patches = \
	$(NULL)
glib_deps = \
	pcre2 \
	libffi \
	zlib \
	$(NULL)
glib_deps_for_build = \
	$(NULL)
glib_options = \
	-Dcocoa=disabled \
	-Dselinux=disabled \
	-Dxattr=false \
	-Dlibmount=disabled \
	-Dtests=false \
	-Dglib_debug=disabled \
	-Dglib_assert=false \
	-Dglib_checks=false \
	--force-fallback-for=pcre \
	$(NULL)
ifeq ($(host_os), $(filter $(host_os),macos ios watchos tvos))
# Use Apple's iconv by default to make our toolchain smaller.
# Our SDK will pull in its own.
glib_options += -Diconv=external
endif
ifeq ($(host_os), $(filter $(host_os),android qnx))
glib_options += -Diconv=external
glib_deps += libiconv
endif
ifeq ($(host_variant), uclibc)
glib_options += -Diconv=external
glib_deps += libiconv
endif

pkg_config_name = pkg-config
pkg_config_version = 4696795673d1d3dec46b663df48f8cbf66461d14
pkg_config_url = $(frida_base_url)/pkg-config.git
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

vala_name = Vala
vala_version = 62ee2b101a5e5f37ce2a073fdb36e7f6ffb553d1
vala_url = $(frida_base_url)/vala.git
vala_recipe = meson
vala_patches = \
	$(NULL)
vala_options = \
	$(NULL)
vala_deps = \
	glib \
	$(NULL)
vala_deps_for_build = \
	$(NULL)

elfutils_name = elfutils
elfutils_version = 1284bbc128473aea220337685985d465607fbac8
elfutils_url = $(frida_base_url)/elfutils.git
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
xz_version = e70f5800ab5001c9509d374dbf3e7e6b866c43fe
xz_url = $(frida_base_url)/xz.git
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
brotli_version = 9f51b6b95599466f46678381492834cdbde018f7
brotli_url = $(frida_base_url)/brotli.git
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
minizip_version = 5879653988db0e09f03952dcd94c1a608b4f681c
minizip_url = $(frida_base_url)/minizip-ng.git
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
ifeq ($(host_os), $(filter $(host_os),macos ios watchos tvos android qnx))
minizip_deps += libiconv
endif
ifeq ($(host_variant), uclibc)
minizip_deps += libiconv
endif

sqlite_name = SQLite
sqlite_version = 87e0535610825f01a033948ba24bbe82db108470
sqlite_url = $(frida_base_url)/sqlite.git
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
libunwind_version = ccd3a38597a8397a3382e4e58fdabb26a6f0be13
libunwind_url = $(frida_base_url)/libunwind.git
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
glib_networking_version = 54a06f8399cac1fbdddd130790475a45a8124304
glib_networking_url = $(frida_base_url)/glib-networking.git
glib_networking_recipe = meson
glib_networking_patches = \
	$(NULL)
glib_networking_options = \
	-Dgnutls=disabled \
	-Dopenssl=enabled \
	-Dlibproxy=disabled \
	-Dgnome_proxy=disabled \
	-Dtests=false \
	$(NULL)
glib_networking_deps = \
	glib \
	openssl \
	$(NULL)
glib_networking_deps_for_build = \
	$(NULL)

libnice_name = libnice
libnice_version = 3c9e960fdb79229b672cbd9e600b4a4f1346409e
libnice_url = $(frida_base_url)/libnice.git
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

lwip_name = lwip
lwip_version = 0c1b36894deeece78eee522278c93483e92d5936
lwip_url = $(frida_base_url)/lwip.git
lwip_recipe = meson
lwip_patches = \
	$(NULL)
lwip_options = \
	-Dlwip_debug=disabled \
	-Dipv4=disabled \
	-Dipv6=enabled \
	-Ddns=disabled \
	-Darp=disabled \
	-Dethernet=disabled \
	-Dtcp_mss=1360 \
	-Dtcp_snd_buf=65535 \
	-Dtcp_wnd=65535 \
	$(NULL)
lwip_deps = \
	glib \
	$(NULL)
lwip_deps_for_build = \
	$(NULL)

usrsctp_name = usrsctp
usrsctp_version = 42627714785294aef2bb31851bdeef5db15f5802
usrsctp_url = $(frida_base_url)/usrsctp.git
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
libgee_version = b1db8f4e0ff72583e5f10205a6512befffa7b541
libgee_url = $(frida_base_url)/libgee.git
libgee_recipe = meson
libgee_patches = \
	$(NULL)
libgee_options = \
	-Ddisable-internal-asserts=true \
	-Ddisable-introspection=true \
	$(NULL)
libgee_deps = \
	glib \
	$(NULL)
libgee_deps_for_build = \
	$(NULL)

json_glib_name = JSON-GLib
json_glib_version = fd29bf6dda9dcf051d2d98838e3086566bf91411
json_glib_url = $(frida_base_url)/json-glib.git
json_glib_recipe = meson
json_glib_patches = \
	$(NULL)
json_glib_options = \
	-Dintrospection=disabled \
	-Dgtk_doc=disabled \
	-Dtests=false \
	-Dnls=disabled \
	$(NULL)
json_glib_deps = \
	glib \
	$(NULL)
json_glib_deps_for_build = \
	$(NULL)

libpsl_name = libpsl
libpsl_version = 579d32b7daf5a0ab1d1fef2d7e15066f52d8d026
libpsl_url = $(frida_base_url)/libpsl.git
libpsl_recipe = meson
libpsl_patches = \
	$(NULL)
libpsl_options = \
	-Druntime=no \
	-Dbuiltin=false \
	-Dtests=false \
	$(NULL)
libpsl_deps = \
	$(NULL)
libpsl_deps_for_build = \
	$(NULL)

libxml2_name = libxml2
libxml2_version = f09ad5551829b7f2df3666759e701644a0ea8558
libxml2_url = $(frida_base_url)/libxml2.git
libxml2_recipe = meson
libxml2_patches = \
	$(NULL)
libxml2_options = \
	-Dhttp=disabled \
	-Dlzma=disabled \
	-Dzlib=disabled \
	$(NULL)
libxml2_deps = \
	$(NULL)
libxml2_deps_for_build = \
	$(NULL)

ngtcp2_name = ngtcp2
ngtcp2_version = 21a9ba1667985ad60e9acafc07af493555deaf53
ngtcp2_url = $(frida_base_url)/ngtcp2.git
ngtcp2_recipe = meson
ngtcp2_patches = \
	$(NULL)
ngtcp2_options = \
	$(NULL)
ngtcp2_deps = \
	openssl \
	$(NULL)
ngtcp2_deps_for_build = \
	$(NULL)

nghttp2_name = nghttp2
nghttp2_version = ae13d24ea59c30e36ca53d1b22c4e664588d0445
nghttp2_url = $(frida_base_url)/nghttp2.git
nghttp2_recipe = meson
nghttp2_patches = \
	$(NULL)
nghttp2_options = \
	$(NULL)
nghttp2_deps = \
	$(NULL)
nghttp2_deps_for_build = \
	$(NULL)

libsoup_name = libsoup
libsoup_version = ecf0161ebfd752746a2f305ca1f506948853526e
libsoup_url = $(frida_base_url)/libsoup.git
libsoup_recipe = meson
libsoup_patches = \
	$(NULL)
libsoup_options = \
	-Dgssapi=disabled \
	-Dntlm=disabled \
	-Dbrotli=enabled \
	-Dtls_check=false \
	-Dintrospection=disabled \
	-Dvapi=disabled \
	-Ddocs=disabled \
	-Dexamples=disabled \
	-Dtests=false \
	-Dsysprof=disabled \
	$(NULL)
libsoup_deps = \
	glib \
	nghttp2 \
	sqlite \
	libpsl \
	brotli \
	$(NULL)
libsoup_deps_for_build = \
	$(NULL)

capstone_name = Capstone
capstone_version = 6ee04f102cbfb6f63c5401a4f346fff87448f97e
capstone_url = $(frida_base_url)/capstone.git
capstone_recipe = meson
capstone_patches = \
	$(NULL)
capstone_options = \
	-Darchs=all \
	-Duse_arch_registration=true \
	-Dx86_att_disable=true \
	-Dcli=disabled \
	$(NULL)
capstone_deps = \
	$(NULL)
capstone_deps_for_build = \
	$(NULL)

quickjs_name = QuickJS
quickjs_version = c81f05c9859cea5f83a80057416a0c7affe9b876
quickjs_url = $(frida_base_url)/quickjs.git
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
openssl_version = 9dd243646cd11180625433c748e914768d9863f4
openssl_url = $(frida_base_url)/openssl.git
openssl_recipe = meson
openssl_patches = \
	$(NULL)
openssl_options = \
	-Dcli=disabled \
	$(NULL)
ifeq ($(host_variant), mingw32)
openssl_options += -Dasm=disabled
endif
openssl_deps = \
	$(NULL)
openssl_deps_for_build = \
	$(NULL)

v8_name = V8
v8_version = bda4a1a3ccc6231a389caebe309fc20fd7cf1650
v8_url = $(frida_base_url)/v8.git
v8_recipe = meson
v8_patches = \
	$(NULL)
v8_options = \
	-Ddebug=false \
	-Dembedder_string=-frida \
	-Dsnapshot_compression=disabled \
	-Dpointer_compression=disabled \
	-Dshared_ro_heap=disabled \
	-Dcppgc_caged_heap=disabled \
	$(NULL)
v8_deps = \
	$(NULL)
v8_deps_for_build = \
	zlib \
	$(NULL)

libcxx_name = libc++
libcxx_version = 2cd34c97d4c79aa45178ebb02734feb7074b7d61
libcxx_url = $(frida_base_url)/libcxx.git
libcxx_recipe = meson
libcxx_patches = \
	$(NULL)
libcxx_options = \
	$(NULL)
libcxx_deps = \
	$(NULL)
libcxx_deps_for_build = \
	$(NULL)


define expand-packages
$(sort $(foreach pkg, $1, $(pkg) $($(subst -,_,$(pkg))_deps) $($(subst -,_,$(pkg))_deps_for_build)))
endef


define make-package-rules

$(foreach pkg, $(call expand-packages,$1), \
	$(if $(findstring meson,$($(subst -,_,$(pkg))_recipe)), $(call make-meson-package-rules,$(pkg),$2)))

endef


define make-meson-package-rules

$(call make-base-package-rules,$1,$2,$(host_machine))

deps/.$1-stamp:
	$$(call grab-and-prepare,$1)
	@touch $$@

build/$2-%/manifest/$1.pkg: build/$2-env-%.rc deps/.$1-stamp \
		$(foreach dep, $($(subst -,_,$1)_deps), build/$2-%/manifest/$(dep).pkg) \
		$(foreach dep, $($(subst -,_,$1)_deps_for_build), build/$2-$(build_machine)/manifest/$(dep).pkg) \
		releng/meson/meson.py
	@$(call print-status,$1,Building)
	@prefix=$$(shell pwd)/build/$2-$$*; \
	builddir=build/$2-tmp-$$*/$1; \
	$(RM) -r $$$$builddir; \
	mkdir -p $$$$builddir; \
	(set -x \
		&& . build/$2-env-$$*.rc \
		&& export PATH="$$(shell pwd)/build/$2-$(build_machine)/bin:$$$$PATH" \
		&& $(call print-status,$1,Configuring) \
		&& meson_args="--native-file build/$2-$(build_machine).txt" \
		&& if [ $$* != $(build_machine) ]; then \
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
	) >>$$$$builddir/build.log 2>&1 || ( \
		echo -en "\n\033[31;1m*** FAILED ***\033[0m"; \
		for log in $$$$builddir/meson-logs/meson-log.txt $$$$builddir/build.log; do \
			if [ -f $$$$log ]; then \
				echo -e "\n\n\033[33;1m   / $$$$log\033[0m"; \
				echo -e "  \033[33;1m|\033[0m"; \
				echo -e "  \033[33;1mv\033[0m\n"; \
				cat $$$$log; \
			fi \
		done; \
		exit 1; \
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
	export PATH="$$(shell pwd)/build/$2-$(build_machine)/bin:$$$$PATH"; \
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
	@sdkroot=build/sdk-$$(host_machine); \
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


define print-repo-banner
	echo -e "\n╭────\n│ 📦 \\033[1m$1\\033[0m\n├───────────────────────────────────────────────╮\n│ URL: $3\n│ CID: $2\n├───────────────────────────────────────────────╯"
endef
