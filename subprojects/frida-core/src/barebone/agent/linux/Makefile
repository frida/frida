# Builds frida-agent.ko in two steps, because kbuild insists on owning the final
# link:
#
#   1. Collapse the Rust staticlib plus everything it needs — the GumJS devkit and
#      the C library — into one relocatable object, and localize every symbol except
#      the entry points frida-kmod.c calls. Localizing matters: Gum and the C library
#      define names like memcpy and malloc that must not leak into the kernel's
#      namespace.
#   2. Hand that object to kbuild as a shipped object, so vermagic, modinfo and
#      CRCs all come from the target kernel's own build system.
#
# See README.md for usage.

# KDIR is the kernel to build against. When it is a source tree that was configured
# elsewhere — which is what Android's modules_prepare tarball gives — point KOUT at
# the prepared tree.
KDIR ?= /lib/modules/$(shell uname -r)/build
PROFILE ?= release

# The kernel's own name for the architecture, and the one Rust knows it by. Both
# follow from the machine this is built for, which defaults to the one building.
ARCH ?= $(shell uname -m \
	| sed -e 's/^i[3-6]86$$/x86/' -e 's/^x86_64$$/x86/' -e 's/^aarch64$$/arm64/')
BITS ?= $(shell uname -m \
	| sed -e 's/^i[3-6]86$$/32/' -e 's/^x86_64$$/64/' -e 's/^aarch64$$/64/')

ifeq ($(ARCH)-$(BITS),x86-32)
RUST_TARGET ?= i686-unknown-none
AGENT_TARGET_FLAGS ?= --target $(AGENT_DIR)/$(RUST_TARGET).json -Z json-target-spec
SDK_ARCH ?= x86
TOOLPREFIX ?=
AGENT_LD_EMULATION ?= elf_i386
else ifeq ($(ARCH)-$(BITS),x86-64)
RUST_TARGET ?= x86_64-unknown-none
SDK_ARCH ?= x86_64
TOOLPREFIX ?=
AGENT_LD_EMULATION ?= elf_x86_64
else ifeq ($(ARCH)-$(BITS),arm64-64)
RUST_TARGET ?= aarch64-unknown-none
SDK_ARCH ?= arm64
TOOLPREFIX ?= aarch64-none-elf-
else
$(error Unsupported ARCH "$(ARCH)" and BITS "$(BITS)"; this flavour covers \
	x86/32, x86/64 and arm64/64)
endif
AGENT_TARGET_FLAGS ?= --target $(RUST_TARGET)

AGENT_DIR := $(CURDIR)/..
AGENT_TARGET_DIR := $(AGENT_DIR)/target-linux
AGENT_LIB := $(AGENT_TARGET_DIR)/$(RUST_TARGET)/$(PROFILE)/libfrida_barebone_agent.a

GUMJS_DEVKIT_DIR ?= $(error Set GUMJS_DEVKIT_DIR to the directory holding libfrida-gumjs.a)

# Any binutils that speak the target's ELF will do; nothing here is compiled with
# them. Building for the host's own architecture, that is the host's binutils.
AGENT_LD ?= $(TOOLPREFIX)ld
AGENT_AR ?= $(TOOLPREFIX)ar
AGENT_NM ?= $(TOOLPREFIX)nm
AGENT_OBJCOPY ?= $(TOOLPREFIX)objcopy

# The ABI the kernel imposes on anything linked into it. The flags only reach crates
# Cargo compiles, and the toolchain ships core, alloc and compiler_builtins prebuilt
# without them, so those three have to be rebuilt from source as well. That needs the
# rust-src component and an unstable flag, hence RUSTC_BOOTSTRAP.
#
# On x86_64 that is endbr64 on every address-taken function, since a kernel with
# CONFIG_X86_KERNEL_IBT faults an indirect call landing anywhere else, and switches
# compiled as comparisons rather than jump tables, which objtool cannot follow into.
# Soft-float, the kernel code model and the absent red zone come with the target
# itself. On arm64
# it is x18, which the kernel reserves for the shadow call stack, so nothing linked in
# may use it as a general-purpose register.
ifeq ($(ARCH)-$(BITS),x86-32)
AGENT_RUSTFLAGS ?=
AGENT_CFLAGS ?= --target=i386-none-elf \
	-Xclang -target-feature -Xclang +soft-float -mno-mmx -mno-sse \
	-mlong-double-64 -fno-pic --sysroot=$(FRIDA_SDK)
else ifeq ($(ARCH)-$(BITS),x86-64)
AGENT_RUSTFLAGS ?= -Zcf-protection=branch \
	-Cllvm-args=--min-jump-table-entries=65536 \
	-Crelocation-model=static -Zplt=yes
AGENT_CFLAGS ?= --target=x86_64-none-elf \
	-Xclang -target-feature -Xclang +soft-float -mno-mmx -mno-sse -mno-red-zone \
	-mlong-double-64 -mcmodel=kernel -fcf-protection=branch -fno-jump-tables \
	-fno-pic --sysroot=$(FRIDA_SDK)
else
AGENT_RUSTFLAGS ?= -Zfixed-x18
AGENT_CFLAGS ?= --target=aarch64-none-elf -mabi=aapcs-soft -mgeneral-regs-only \
	-ffixed-x18 -fno-pic --sysroot=$(FRIDA_SDK)
endif
AGENT_BUILD_STD ?= -Z build-std=core,alloc,compiler_builtins

# The devkit header reaches for GLib's and capstone's, which a devkit built against
# an SDK leaves in that SDK rather than beside itself. The sysroot's own include
# directory is spelled out because only clang's bare-metal driver, which it selects
# for arm64 but not for x86_64, searches there on its own.
AGENT_CFLAGS += -isystem $(FRIDA_SDK)/include \
	-I$(FRIDA_SDK)/include/glib-2.0 \
	-I$(FRIDA_SDK)/lib/glib-2.0/include \
	-I$(FRIDA_SDK)/include/capstone \
	-I$(FRIDA_SDK)/include/json-glib-1.0

# The soft-float SDK, whose libc is picolibc and whose compiler runtime is
# compiler-rt. GCC's own newlib and libgcc are hard-float and cannot be used here.
FRIDA_SDK ?= $(error Set FRIDA_SDK to a none-$(SDK_ARCH)-softfloat_nopic SDK prefix)
LIBM = $(FRIDA_SDK)/lib/libm.a
LIBBUILTINS = $(FRIDA_SDK)/lib/libclang_rt.builtins.a

# bindgen reads the devkit header through whatever this compiler reports as its
# include path, so it has to be one that knows where the SDK keeps its libc. It has
# to be clang either way: -mabi=aapcs-soft landed there in 19 and GCC has it at no
# version, and on x86_64 the soft-float ABI is reachable only as an -Xclang feature.
AGENT_CC ?= clang

PRELINKED := frida-agent-prelinked.o.bin

AGENT_LIB_SOFTFLOAT := $(AGENT_TARGET_DIR)/libfrida-barebone-agent-softfloat.a

# On arm64 the toolchain's own compiler_builtins is compiled for the ordinary AAPCS,
# where a double travels in d0: its __muldf3 opens with "fmov x14, d0". Everything
# else there is -mabi=aapcs-soft and passes that same double in x0. Whole-archiving
# the staticlib puts those definitions in front of the SDK's compiler-rt, which has
# the soft-float ones, and every double the C library computes then comes back
# garbage — quietly, since the arithmetic still runs. Drop them and let compiler-rt
# answer instead. x86_64 needs none of this: soft-float is part of the Rust target
# there, so the copy build-std compiles already passes doubles the way the C half
# expects.
ifeq ($(ARCH),arm64)
DROP_HARDFLOAT_BUILTINS = \
	$(AGENT_AR) d $@ $$($(AGENT_NM) --print-armap $@ | sed -n 's/^__muldf3 in //p') && \
	$(AGENT_AR) s $@
else
DROP_HARDFLOAT_BUILTINS = :
endif

# The agent puts malloc, the C library's locks and its process stubs on the kernel's
# own facilities, so picolibc's versions are duplicate definitions rather than
# fallbacks. Work out which members they live in instead of naming them: the set
# follows from what the agent defines, and picolibc moves things between releases.
LIBC := $(AGENT_TARGET_DIR)/libc-trimmed.a

ENTRY_POINTS := frida_agent_start frida_agent_stop \
	gum_interceptor_obtain gum_interceptor_begin_transaction \
	gum_interceptor_end_transaction gum_interceptor_replace \
	gum_interceptor_revert
KEEP_GLOBAL := $(foreach s,$(ENTRY_POINTS),--keep-global-symbol=$(s))

# Two structures the reparenting path reaches into lost a member upstream, and
# neither change announces itself as anything frida-kmod.c could test for. The
# headers it is compiled against are the authority on both, so ask them.
FRIDA_KMOD_DEFS := \
	$(if $(shell grep -l 'list_head[[:space:]]*thread_group;' \
		$(KDIR)/include/linux/sched.h),-DFRIDA_HAVE_TASK_THREAD_GROUP) \
	$(if $(shell grep -l 'spinlock_t[[:space:]]*lock;' \
		$(KDIR)/include/linux/fs_struct.h),-DFRIDA_HAVE_FS_STRUCT_LOCK)

all: modules

# A module that dies in its init path is left wedged under its own name until the
# next boot, so bring-up wants a fresh one per attempt.
MODULE_NAME ?= frida-agent

modules: $(PRELINKED)
	$(MAKE) -C $(KDIR) $(if $(KOUT),O=$(KOUT)) M=$(CURDIR) ARCH=$(ARCH) \
		FRIDA_KMOD_DEFS="$(FRIDA_KMOD_DEFS)" MODULE_NAME=$(MODULE_NAME) modules

$(PRELINKED): $(AGENT_LIB_SOFTFLOAT) $(LIBC) prelink.lds
	$(AGENT_LD) -r $(if $(AGENT_LD_EMULATION),-m $(AGENT_LD_EMULATION)) \
		-T prelink.lds -o $@.tmp \
		--whole-archive $(AGENT_LIB_SOFTFLOAT) --no-whole-archive \
		--start-group \
			$(GUMJS_DEVKIT_DIR)/libfrida-gumjs.a \
			$(LIBC) \
			$(LIBM) \
			$(LIBBUILTINS) \
		--end-group
	$(AGENT_OBJCOPY) $(KEEP_GLOBAL) --strip-debug \
		--remove-section=.llvmbc --remove-section=.llvmcmd $@.tmp $@
	rm -f $@.tmp

$(AGENT_LIB_SOFTFLOAT): agent
	cp $(AGENT_LIB) $@
	$(DROP_HARDFLOAT_BUILTINS)

$(LIBC): export LC_ALL = C
$(LIBC): $(AGENT_LIB_SOFTFLOAT)
	cp $(FRIDA_SDK)/lib/libc.a $@
	$(AGENT_NM) --defined-only $(AGENT_LIB_SOFTFLOAT) \
		| awk 'NF == 3 && $$2 ~ /^[TDBR]$$/ { print $$3 }' | sort -u > $@.agent-syms
	$(AGENT_NM) --print-armap $@ | sed -n 's/^\([^ ]*\) in \(.*\)$$/\1 \2/p' | sort -u > $@.members
	$(AGENT_AR) d $@ $$(join $@.agent-syms $@.members | awk '{ print $$2 }' | sort -u)
	$(AGENT_AR) s $@
	rm -f $@.agent-syms $@.members

agent:
	cd $(AGENT_DIR) && RUSTC_BOOTSTRAP=1 RUSTFLAGS="$(AGENT_RUSTFLAGS)" \
		CC_$(subst -,_,$(RUST_TARGET))="$(AGENT_CC) $(AGENT_CFLAGS)" \
		cargo build --$(PROFILE) \
		$(AGENT_TARGET_FLAGS) --features linux \
		--target-dir $(AGENT_TARGET_DIR) $(AGENT_BUILD_STD)

clean:
	$(MAKE) -C $(KDIR) $(if $(KOUT),O=$(KOUT)) M=$(CURDIR) ARCH=$(ARCH) clean
	rm -f $(PRELINKED) $(PRELINKED).tmp $(AGENT_LIB_SOFTFLOAT) $(LIBC)

.PHONY: all modules agent clean
