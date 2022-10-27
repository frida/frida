frida_deps_version = 20221021
frida_bootstrap_version = 20220130


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
libffi_version = 763cf41612c4a9ed98d764a5237acdb9f5337f2d
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

selinux_name = SELinux Userspace
selinux_version = 9c7ba053bb075cace088d268fda400f6bc4ab14c
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
glib_version = 531183c332f874ea2d792c4c250d4599c07e60c0
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
ifeq ($(FRIDA_LIBC), uclibc)
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
vala_version = d07b689485b3c79116a569696d36ad7c0e299c02
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
xz_version = d4bb4f6c690844cf34c19104f5d766f066334be7
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
brotli_version = 8abf3188d1ef4bb8a633f894fec731bdd510ee49
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
minizip_version = 535c1b9150e5e47b9a533b6f16787042da92ac63
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
ifeq ($(FRIDA_LIBC), uclibc)
minizip_deps += libiconv
endif

sqlite_name = SQLite
sqlite_version = 6b876d7c22f10488477d106dfe51f3fbd4ce2d20
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
libunwind_version = 12ac8fe53a2cb23501116a83ee59bd57da06bfe9
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
glib_networking_version = 05d01a444f1738b3cfe2d583f49fdba1357e3184
glib_networking_url = $(frida_base_url)/glib-networking.git
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
usrsctp_version = e82326e75acb84668d13e804d1b8a1c1530e053e
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
libgee_version = 5b00fd64096b369a04fe04a7246e1927c3fedbd7
libgee_url = $(frida_base_url)/libgee.git
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
libxml2_version = 35b8e0616b9f820c488eabd402e9d4097454997f
libxml2_url = $(frida_base_url)/libxml2.git
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
libsoup_version = 72e27e0dbcce1e448d31cbc06b0b17d42a277b85
libsoup_url = $(frida_base_url)/libsoup.git
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
	-Dexamples=disabled \
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
quickjs_version = a3303a2bec40fb55df6de5e94e53a7a67e7dbfb0
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
openssl_version = bf78536ec5dc0f834f3bf61c11e12c8a70c52bd2
openssl_url = $(frida_base_url)/openssl.git
openssl_recipe = meson
openssl_patches = \
	$(NULL)
openssl_options = \
	-Dcli=disabled \
	$(NULL)
openssl_deps = \
	$(NULL)
openssl_deps_for_build = \
	$(NULL)

v8_name = V8
v8_version = f0129b2e7d1ebdd93419f0f93435c7897561fa93
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
