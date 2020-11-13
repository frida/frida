frida_toolchain_version := 20201106
frida_sdk_version := 20201106
frida_bootstrap_version := 20201028


frida_base_url := https://github.com/frida
gnu_mirror := saimei.ftp.acc.umu.se/mirror/gnu.org/gnu


build_os := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
build_arch := $(shell releng/detect-arch.sh)
build_os_arch := $(build_os)-$(build_arch)

ifdef FRIDA_HOST
	host_os := $(shell echo $(FRIDA_HOST) | cut -f1 -d"-")
	host_arch := $(shell echo $(FRIDA_HOST) | cut -f2 -d"-")
else
	host_os := $(build_os)
	host_arch := $(build_arch)
endif
host_os_arch := $(host_os)-$(host_arch)


m4_version := 1.4.18
m4_url := https://$(gnu_mirror)/m4/m4-$(m4_version).tar.gz
m4_hash := ab2633921a5cd38e48797bf5521ad259bdc4b979078034a3b790d7fec5493fab
m4_patches := \
	m4-vasnprintf-apple-fix.patch \
	m4-ftbfs-fix.patch \
	$(NULL)
m4_options := \
	$(NULL)

autoconf_version := 2.69
autoconf_url := https://$(gnu_mirror)/autoconf/autoconf-$(autoconf_version).tar.gz
autoconf_hash := 954bd69b391edc12d6a4a51a2dd1476543da5c6bbf05a95b59dc0dd6fd4c2969
autoconf_patches := \
	autoconf-uclibc.patch \
	$(NULL)
autoconf_options := \
	$(NULL)

automake_version := 1.16.2
automake_url := https://$(gnu_mirror)/automake/automake-$(automake_version).tar.gz
automake_hash := b2f361094b410b4acbf4efba7337bdb786335ca09eb2518635a09fb7319ca5c1
automake_patches := \
	$(NULL)
automake_options := \
	$(NULL)
automake_api_version := 1.16

libtool_version := 2.4.6
libtool_url := https://$(gnu_mirror)/libtool/libtool-$(libtool_version).tar.gz
libtool_hash := e3bd4d5d3d025a36c21dd6af7ea818a2afcd4dfc1ea5a17b39d7854bcd0c06e3
libtool_patches := \
	libtool-fixes.patch \
	$(NULL)
libtool_options := \
	$(NULL)

gettext_version := 0.21
gettext_url := https://$(gnu_mirror)/gettext/gettext-$(gettext_version).tar.gz
gettext_hash := c77d0da3102aec9c07f43671e60611ebff89a996ef159497ce8e59d075786b12
gettext_patches := \
	gettext-static-only.patch \
	$(NULL)
gettext_options := \
	--disable-curses \
	$(NULL)

zlib_version := 91920caec2160ffd919fd48dc4e7a0f6c3fb36d2
zlib_url := $(frida_base_url)/zlib.git
zlib_options := \
	$(NULL)

libffi_version := 4612f7f4b8cfda9a1f07e66d033bb9319860af9b
libffi_options := \
	$(NULL)

glib_version := 327fd7518d5612492723aec20c97fd2505e98fd8
ifeq ($(host_os), $(filter $(host_os),macos ios android qnx))
	glib_iconv_option := -Diconv=external
endif
ifeq ($(FRIDA_LIBC), uclibc)
	glib_iconv_option := -Diconv=external
endif
glib_options := \
	$(glib_iconv_option) \
	-Dselinux=disabled \
	-Dxattr=false \
	-Dlibmount=disabled \
	-Dinternal_pcre=true \
	-Dtests=false \
	$(NULL)

pkg_config_version := b7fb5edc1f1a4fb17cd5cb94f4cf21912184da43
pkg_config_options := \
	$(NULL)

flex_version := 2.6.4
flex_url := https://github.com/westes/flex/releases/download/v$(flex_version)/flex-$(flex_version).tar.gz
flex_hash := e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995
flex_patches := \
	flex-modern-glibc.patch \
	$(NULL)
flex_options := \
	$(NULL)

bison_version := 3.7.3
bison_url := https://$(gnu_mirror)/bison/bison-$(bison_version).tar.gz
bison_hash := 104fe912f2212ab4e4a59df888a93b719a046ffc38d178e943f6c54b1f27b3c7
bison_patches := \
	$(NULL)
bison_options := \
	$(NULL)

vala_version := 5067d99e7b9b8ab1c9393f70596fc5bd4f8b46a2
vala_options := \
	$(NULL)

libiconv_version := 1.16
libiconv_url := https://$(gnu_mirror)/libiconv/libiconv-$(libiconv_version).tar.gz
libiconv_hash := e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04
libiconv_options := \
	$(NULL)

elfutils_version := elfutils-0.182
elfutils_patches := \
	elfutils-clang.patch \
	elfutils-android.patch \
	elfutils-glibc-compatibility.patch \
	elfutils-musl.patch \
	$(NULL)
elfutils_options := \
	--enable-maintainer-mode \
	--disable-libdebuginfod \
	$(NULL)

libdwarf_version := 20201020
libdwarf_url := https://www.prevanders.net/libdwarf-$(libdwarf_version).tar.gz
libdwarf_hash := 1c5ce59e314c6fe74a1f1b4e2fa12caea9c24429309aa0ebdfa882f74f016eff
libdwarf_options := \
	$(NULL)

xz_version := 6c84113065f603803683d30342207c73465bbc12
xz_options := \
	$(NULL)

sqlite_version := 9f21a054d5c24c2036e9d1b28c630ecda5ae24c3
sqlite_options := \
	$(NULL)

libunwind_version := 66ca44cd82389cd7cfbbd482e58324a79f6679ab
libunwind_options := \
	$(NULL)

glib_networking_version := 7be8c21840cd4eb23477dc0c0d261f85d2c57778
glib_networking_options := \
	-Dgnutls=disabled \
	-Dopenssl=enabled \
	-Dlibproxy=disabled \
	-Dgnome_proxy=disabled \
	-Dstatic_modules=true \
	$(NULL)

libgee_version := c7e96ac037610cc3d0e11dc964b7b1fca479fc2a
libgee_options := \
	$(NULL)

json_glib_version := 9dd3b3898a2c41a1f9af24da8bab22e61526d299
json_glib_options := \
	-Dintrospection=disabled \
	-Dtests=false \
	$(NULL)

libpsl_version := 3caf6c33029b6c43fc31ce172badf976f6c37bc4
libpsl_options := \
	-Dtests=false \
	$(NULL)

libxml2_version := f1845f6fd1c0b6aac0f573c77a8250f8d4eb31fd
libxml2_options := \
	$(NULL)

libsoup_version := d0ac83f3952d590da09968b1e30736782b1d1c7f
libsoup_options := \
	-Dgssapi=disabled \
	-Dtls_check=false \
	-Dgnome=false \
	-Dintrospection=disabled \
	-Dvapi=disabled \
	-Dtests=false \
	-Dsysprof=disabled \
	$(NULL)

capstone_version := 03295d19d2c3b0162118a6d9742312301cde1d00
capstone_options := \
	-Darchs=$(shell echo $(host_arch) | sed 's,^x86_64$$,x86,') \
	-Dx86_att_disable=true \
	-Dcli=disabled \
	$(NULL)

quickjs_version := 26ce42ea32a3318b1c6318d4db6cf01ade54be61
quickjs_options := \
	-Dlibc=false \
	-Dbignum=true \
	-Datomics=disabled \
	-Dstack_check=disabled \
	$(NULL)

tinycc_version := c9f502d1a14aca5209eae8bdc9de1784ba27cb6f
tinycc_options := \
	$(NULL)

openssl_version := 1.1.1h
openssl_url := https://www.openssl.org/source/openssl-$(openssl_version).tar.gz
openssl_hash := 5c9ca8774bd7b03e5784f26ae9e9e6d749c9da2438545077e6b3d755a06595d9
openssl_options := \
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

v8_version := 6e74fda056e277e0c80240e7a4dd76c7225bd9ec
v8_api_version := 8.0
v8_options := \
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
gn_version := 75194c124f158d7fabdc94048f1a3f850a5f0701
depot_tools_version := b674f8a27725216bd2201652636649d83064ca4a


define clone-and-prepare
	@$(RM) -r ext/$1

	@url=$($(subst -,_,$1)_url) \
		&& version=$($(subst -,_,$1)_version) \
		&& echo -e "╭────"\
		&& echo -e "│ 🔨 \\033[1m$1\\033[0m" \
		&& echo -e "├───────────────────────────────────────────────╮" \
		&& echo -e "│ URL: $$url" \
		&& echo -e "│ CID: $$version" \
		&& echo -e "└───────────────────────────────────────────────╯" \
		&& git clone --recurse-submodules $$url ext/$1 \
		&& cd ext/$1 \ \
		&& git checkout -q $$version

	$(call apply-patches,$1)
endef

define download-and-prepare
	@$(RM) -r ext/$1
	@mkdir -p ext/$1

	@url=$($(subst -,_,$1)_url) \
		&& version=$($(subst -,_,$1)_version) \
		&& expected_hash=$($(subst -,_,$1)_hash) \
		&& echo -e "╭────"\
		&& echo -e "│ 🔨 \\033[1m$1\\033[0m $$version" \
		&& echo -e "├─────\
		&& echo -e "│ URL: $$url" \
		&& echo -e "│ SHA: $$expected_hash" \
		&& echo -e "└──────\
		&& if command -v curl >/dev/null; then \
			curl -sSfLo ext/.$1-tarball $$url; \
		else \
			wget -qO ext/.$1-tarball $$url; \
		fi \
		&& actual_hash=$$(shasum -a 256 -b ext/.$1-tarball | awk '{ print $$1; }') \
		&& case $$actual_hash in \
			$$expected_hash) \
				;; \
			*) \
				echo "$1 tarball is corrupted; its hash is: $$actual_hash"; \
				exit 1; \
				;; \
		esac

	@echo "Extracting to ext/$1"
	@tar -C ext/$1 -x -f ext/.$1-tarball -z --strip-components 1

	$(call apply-patches,$1)

	@rm ext/.$1-tarball
endef

define apply-patches
	@cd ext/$1 \
		&& for patch in $($(subst -,_,$1)_patches); do \
			echo "Applying \\033[1m$$patch\\033[0m"; \
			patch -p1 < ../../releng/patches/$$patch || exit 1; \
		done
endef
