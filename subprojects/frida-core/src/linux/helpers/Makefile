top_srcdir := ../../..
releng := $(top_srcdir)/releng

BUILDDIR ?= $(top_srcdir)/build
MESON ?= $(releng)/meson/meson.py

ifeq ($(shell uname -s),Darwin)
KERNEL_MAKE ?= gmake
KERNEL_SED ?= gsed
KERNEL_AWK ?= gawk
BPF_CLANG ?= /Applications/AndroidNDK14206865.app/Contents/NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang
else
KERNEL_MAKE ?= make
KERNEL_SED ?= sed
KERNEL_AWK ?= awk
BPF_CLANG ?= clang
endif

ifdef FRIDA_HOST
host_machine := $(FRIDA_HOST)
else
host_machine := $(shell basename $$(ls -1 $(BUILDDIR)/*/frida-linux-*.txt | tail -1) | cut -d"." -f1 | cut -d"-" -f2-)
endif
host_arch := $(shell echo $(host_machine) | cut -d"-" -f2)

crossfile := $(BUILDDIR)/$(host_machine)/frida-$(host_machine).txt
valac := $(shell grep "vala = " $(crossfile) | cut -c9- | sed -e "s/.$$//" -e "s/,//g")

artifacts_dir := artifacts
build_dir := build

native_outdir := $(artifacts_dir)/native/$(host_arch)

bpf_outdir := $(artifacts_dir)/bpf
bpf_noarch_progs := \
	activity-sampler \
	$(NULL)
bpf_arch_progs := \
	spawn-gater \
	syscall-tracer \
	$(NULL)
bpf_noarch_pairs := \
	big:bpfeb \
	little:bpfel
bpf_arch_pairs := \
	x86:bpfel x86_64:bpfel \
	arm:bpfel armbe:bpfeb arm64:bpfel arm64be:bpfeb \
	mips:bpfeb mipsel:bpfel mips64:bpfeb mips64el:bpfel \
	$(NULL)

build-native: ext/linux/tools/include/nolibc/nolibc.h
	rm -rf build
	$(MESON) setup --prefix /usr --cross-file $(crossfile) -Db_lto=true build
	DESTDIR=$(abspath build/dist) $(MESON) install -C build
	for f in build/dist/usr/share/frida-helpers/*; do \
		install -m644 $$f $(native_outdir)/; \
	done

bpf_targets :=
bpf_obj := $(build_dir)/module.o

kernel_headers_dir := ext/kernel-headers
noarch_uapi_arch ?= x86

linux_uapi_arch = $(strip \
	$(if $(filter x86 x86_64,$1),x86, \
	$(if $(filter arm armbe,$1),arm, \
	$(if $(filter arm64 arm64be,$1),arm64, \
	$(if $(filter mips mipsel mips64 mips64el,$1),mips,unknown)))))
has_compat32 = $(strip \
	$(if $(filter x86_64 arm64 arm64be,$(1)),1,))

kernel_headers_stamp = $(kernel_headers_dir)/$(1)/.stamp
kernel_headers_for   = $(call kernel_headers_stamp,$(call linux_uapi_arch,$(1)))

kernel_uapi_arches := x86 arm arm64 mips
kernel_uapi_stamps := $(foreach a,$(kernel_uapi_arches),$(call kernel_headers_stamp,$(a)))

$(call kernel_headers_stamp,%): ext/linux/tools/include/nolibc/nolibc.h
	@mkdir -p $(kernel_headers_dir)/$*
	@$(KERNEL_MAKE) -C ext/linux \
		ARCH=$* \
		SED=$(KERNEL_SED) \
		AWK=$(KERNEL_AWK) \
		headers_install INSTALL_HDR_PATH=$(abspath $(kernel_headers_dir)/$*)
	@touch $@

libbpf_devel_url := https://kojipkgs.fedoraproject.org//packages/libbpf/1.6.3/1.fc44/x86_64/libbpf-devel-1.6.3-1.fc44.x86_64.rpm
libbpf_devel_rpm := $(notdir $(libbpf_devel_url))
libbpf_devel_dir := ext/libbpf

define recreate_build_dir
	rm -rf $(build_dir)
	mkdir -p $(build_dir)
endef

define bpf_compile
	$(BPF_CLANG) \
		-target $(1) \
		-D__TARGET_ARCH_$(2) \
		$(if $(call has_compat32,$(3)),-DFRIDA_HAS_COMPAT32=1,) \
		-O2 -g \
		-I$(build_dir) \
		-I$(libbpf_devel_dir) \
		-isystem $(kernel_headers_dir)/$(2)/include \
		-isystem $(kernel_headers_dir)/$(2)/include/uapi \
		-isystem $(kernel_headers_dir)/$(2)/include/asm-generic \
		$< \
		-c \
		-o $(bpf_obj)
	mkdir -p $(@D)
	install -m644 $(bpf_obj) $@
endef

define declare_bpf_noarch
bpf_targets += $(foreach p,$(bpf_noarch_progs),$(bpf_outdir)/noarch-$(1)/$(p).elf)

$(bpf_outdir)/noarch-$(1)/%.elf: %.bpf.c $(call kernel_headers_stamp,$(noarch_uapi_arch)) $(libbpf_devel_dir)
	$$(call recreate_build_dir)
	$$(call bpf_compile,$(2),$(noarch_uapi_arch),noarch)
endef

define declare_bpf_arch
bpf_targets += $(foreach p,$(bpf_arch_progs),$(bpf_outdir)/$(1)/$(p).elf)

$(bpf_outdir)/$(1)/%.elf: %.bpf.c $(call kernel_headers_for,$(1)) $(libbpf_devel_dir)
	$$(call recreate_build_dir)
	syscalls_dir=$$(realpath $(top_srcdir)/lib/base/linux-syscalls); \
		cd build && \
		$$(valac) \
			-C \
			--header=frida-linux-syscalls.h \
			--profile=posix \
			$$$${syscalls_dir}/$(shell echo $(1) | sed -e 's/be$$//' -e 's/el$$//').vala && \
		$(KERNEL_AWK) ' \
			/typedef[[:space:]]+enum[[:space:]]*\{/ { in_enum=1; next } \
			in_enum && /^[[:space:]]*}/ { in_enum=0; next } \
			in_enum { \
				if (match($$$$0, /^[[:space:]]*([A-Z0-9_]+)[[:space:]]*=[[:space:]]*([^,[:space:]]+)[[:space:]]*,?[[:space:]]*$$$$/, m)) \
					print "#define " m[1] " " m[2]; \
				next; \
			} \
			{ print } \
		' frida-linux-syscalls.h > frida-linux-syscalls.h.tmp && \
		mv frida-linux-syscalls.h.tmp frida-linux-syscalls.h
	$$(call bpf_compile,$(2),$$(call linux_uapi_arch,$(1)),$(1))
endef

pair_abi    = $(word 1,$(subst :, ,$1))
pair_target = $(word 2,$(subst :, ,$1))

$(foreach pair,$(bpf_noarch_pairs), \
	$(eval $(call declare_bpf_noarch,$(call pair_abi,$(pair)),$(call pair_target,$(pair)))))

$(foreach pair,$(bpf_arch_pairs), \
	$(eval $(call declare_bpf_arch,$(call pair_abi,$(pair)),$(call pair_target,$(pair)))))

build-bpf: $(bpf_targets)

ext/linux/tools/include/nolibc/nolibc.h: patches/linux-headers-install-portability.patch patches/linux-nolibc-tweaks.patch
	if [ -d ext/linux ]; then \
		cd ext/linux; \
		git reset --hard; \
		git checkout v6.2; \
	else \
		git clone --branch=v6.2 --depth=1 https://github.com/torvalds/linux.git ext/linux; \
	fi
	for fix in $^; do \
		patch -d ext/linux -p1 < $${fix}; \
	done
	@touch -c $@

$(libbpf_devel_dir):
	mkdir -p $(libbpf_devel_dir)
	curl -L -o $(libbpf_devel_rpm) $(libbpf_devel_url)
	rpm2cpio -f pax $(libbpf_devel_rpm) | tar -xpf - \
		-C $(libbpf_devel_dir) \
		--strip-components=3
	rm -f $(libbpf_devel_rpm)

.PHONY: build-native build-bpf
