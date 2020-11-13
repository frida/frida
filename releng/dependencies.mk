frida_toolchain_version := 20201106
frida_sdk_version := 20201106
frida_bootstrap_version := 20201028


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
zlib_options :=

libffi_version := 4612f7f4b8cfda9a1f07e66d033bb9319860af9b
libffi_options :=

glib_version := 327fd7518d5612492723aec20c97fd2505e98fd8
ifeq ($(host_os), $(filter $(host_os),macos ios android qnx))
	glib_iconv_option := -Diconv=external
endif
ifeq ($(FRIDA_LIBC), uclibc)
	glib_iconv_option := -Diconv=external
endif
glib_options := $(glib_iconv_option) -Dselinux=disabled -Dxattr=false -Dlibmount=disabled -Dinternal_pcre=true -Dtests=false

pkg_config_version := b7fb5edc1f1a4fb17cd5cb94f4cf21912184da43
pkg_config_options :=

flex_version := 2.6.4
flex_hash := e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995

bison_version := 3.7.3
bison_hash := 104fe912f2212ab4e4a59df888a93b719a046ffc38d178e943f6c54b1f27b3c7

vala_version := 5067d99e7b9b8ab1c9393f70596fc5bd4f8b46a2
vala_options :=

libiconv_version := 1.16
libiconv_hash := e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04

elfutils_version := elfutils-0.182

libdwarf_version := 20201020
libdwarf_hash := 1c5ce59e314c6fe74a1f1b4e2fa12caea9c24429309aa0ebdfa882f74f016eff

xz_version := 6c84113065f603803683d30342207c73465bbc12

sqlite_version := 9f21a054d5c24c2036e9d1b28c630ecda5ae24c3
sqlite_options :=

libunwind_version := 66ca44cd82389cd7cfbbd482e58324a79f6679ab

glib_networking_version := 7be8c21840cd4eb23477dc0c0d261f85d2c57778
glib_networking_options := -Dgnutls=disabled -Dopenssl=enabled -Dlibproxy=disabled -Dgnome_proxy=disabled -Dstatic_modules=true

libgee_version := c7e96ac037610cc3d0e11dc964b7b1fca479fc2a
libgee_options :=

json_glib_version := 9dd3b3898a2c41a1f9af24da8bab22e61526d299
json_glib_options := -Dintrospection=disabled -Dtests=false

libpsl_version := 3caf6c33029b6c43fc31ce172badf976f6c37bc4
libpsl_options := -Dtests=false

libxml2_version := f1845f6fd1c0b6aac0f573c77a8250f8d4eb31fd
libxml2_options :=

libsoup_version := d0ac83f3952d590da09968b1e30736782b1d1c7f
libsoup_options := -Dgssapi=disabled -Dtls_check=false -Dgnome=false -Dintrospection=disabled -Dvapi=disabled -Dtests=false -Dsysprof=disabled

capstone_version := 03295d19d2c3b0162118a6d9742312301cde1d00
capstone_options := -Darchs=$(shell echo $(host_arch) | sed 's,^x86_64$$,x86,') -Dx86_att_disable=true -Dcli=disabled

quickjs_version := 26ce42ea32a3318b1c6318d4db6cf01ade54be61
quickjs_options := -Dlibc=false -Dbignum=true -Datomics=disabled -Dstack_check=disabled

tinycc_version := c9f502d1a14aca5209eae8bdc9de1784ba27cb6f
tinycc_options :=

openssl_version := 1.1.1h
openssl_hash := 5c9ca8774bd7b03e5784f26ae9e9e6d749c9da2438545077e6b3d755a06595d9

v8_version := 6e74fda056e277e0c80240e7a4dd76c7225bd9ec
v8_api_version := 8.0
gn_version := 75194c124f158d7fabdc94048f1a3f850a5f0701
depot_tools_version := b674f8a27725216bd2201652636649d83064ca4a


repo_base_url := https://github.com/frida
repo_suffix := .git

gnu_mirror := saimei.ftp.acc.umu.se/mirror/gnu.org/gnu


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
