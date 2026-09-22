
PROFILE ?= release
ARCH ?= arm64e

ifeq ($(ARCH),arm64e)
RUST_TARGET ?= arm64e-apple-darwin
CLANG_TARGET ?= arm64e-apple-macos11
else ifeq ($(ARCH),arm64)
RUST_TARGET ?= aarch64-apple-darwin
CLANG_TARGET ?= arm64-apple-macos11
else
RUST_TARGET ?= x86_64-apple-darwin
CLANG_TARGET ?= x86_64-apple-macos11
endif

AGENT_DIR := $(CURDIR)/..
AGENT_TARGET_DIR := $(AGENT_DIR)/target-xnu-kext
AGENT_LIB := $(AGENT_TARGET_DIR)/$(RUST_TARGET)/$(PROFILE)/libfrida_barebone_agent.a

SDK ?= $(shell xcrun --sdk macosx --show-sdk-path)
KERNEL_HEADERS := $(SDK)/System/Library/Frameworks/Kernel.framework/Headers

FRIDA_VERSION := $(or $(FRIDA_VERSION),0.0.0)
BUNDLE_VERSION := $(firstword $(subst -, ,$(FRIDA_VERSION)))

FRIDA_SDK ?= $(error Set FRIDA_SDK to a macos-$(ARCH)-kernel SDK prefix)
GUMJS_DEVKIT_DIR ?= $(error Set GUMJS_DEVKIT_DIR to the directory holding libfrida-gumjs.a)

AGENT_RUSTFLAGS ?= -Cembed-bitcode=no
AGENT_BUILD_STD ?= -Z build-std=core,alloc,compiler_builtins

GLUE_CFLAGS ?= -target $(CLANG_TARGET) -isysroot $(SDK) -I$(KERNEL_HEADERS) \
	-mkernel -fno-builtin -fno-common -nostdinc -std=gnu11 \
	-DKERNEL -DKERNEL_PRIVATE -DAPPLE -D__MACHO__=1

AGENT_CC ?= clang -target $(CLANG_TARGET) -mkernel -nostdlibinc \
	-isystem $(FRIDA_SDK)/include \
	-I$(FRIDA_SDK)/include/glib-2.0 \
	-I$(FRIDA_SDK)/lib/glib-2.0/include \
	-I$(FRIDA_SDK)/include/capstone \
	-I$(FRIDA_SDK)/include/json-glib-1.0

stamp_version = sed -e 's/@FRIDA_VERSION@/$(FRIDA_VERSION)/' \
	-e 's/@BUNDLE_VERSION@/$(BUNDLE_VERSION)/' Info.plist

BUNDLE := frida-agent.kext
BINARY := $(BUNDLE)/Contents/MacOS/frida-agent

all: $(BUNDLE)

skeleton: frida-kext.o Info.plist
	mkdir -p $(BUNDLE)/Contents/MacOS
	$(stamp_version) > $(BUNDLE)/Contents/Info.plist
	$(shell xcrun --find ld) -kext -arch $(ARCH) -o $(BINARY) frida-kext.o

$(BUNDLE): frida-agent Info.plist
	mkdir -p $(BUNDLE)/Contents/MacOS
	$(stamp_version) > $(BUNDLE)/Contents/Info.plist
	cp frida-agent $(BINARY)

frida-agent: frida-kext.o frida-signing.o agent
	$(shell xcrun --find ld) -kext -arch $(ARCH) -o $@ \
		frida-kext.o \
		frida-signing.o \
		$(AGENT_LIB) \
		$(GUMJS_DEVKIT_DIR)/libfrida-gumjs.a \
		$(FRIDA_SDK)/lib/libc.a \
		$(FRIDA_SDK)/lib/libm.a \
		$(FRIDA_SDK)/lib/libclang_rt.builtins.a

frida-kext.o: frida-kext.c
	$(shell xcrun --find clang) $(GLUE_CFLAGS) -c $< -o $@

frida-signing.o: frida-signing.c
	$(AGENT_CC) -I$(GUMJS_DEVKIT_DIR) -c $< -o $@

agent:
	cd $(AGENT_DIR) && RUSTC_BOOTSTRAP=1 RUSTFLAGS="$(AGENT_RUSTFLAGS)" \
		CC_$(subst -,_,$(RUST_TARGET))="$(AGENT_CC)" \
		GUMJS_DEVKIT_DIR="$(GUMJS_DEVKIT_DIR)" \
		cargo build --$(PROFILE) \
		--target $(RUST_TARGET) --features xnu-kext \
		--target-dir $(AGENT_TARGET_DIR) $(AGENT_BUILD_STD)

clean:
	rm -rf $(BUNDLE) frida-agent frida-kext.o frida-signing.o

.PHONY: all agent clean skeleton
