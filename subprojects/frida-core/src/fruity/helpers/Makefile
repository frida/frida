helpers := \
	build/symbol-fetcher.dylib build/test-symbol-fetcher \
	build/symbol-resolver.dylib build/test-symbol-resolver \
	build/upload-listener.dylib build/test-upload-listener \
	build/upload-receiver.dylib build/test-upload-receiver \
	$(NULL)

sources := \
	symbol-fetcher.c \
	symbol-resolver.c \
	upload-listener.c \
	upload-receiver.c \
	$(NULL)

macos_cc := $(shell xcrun --sdk macosx -f clang)
ios_cc := $(shell xcrun --sdk iphoneos -f clang)
cflags := -Wall -Werror -pipe -Oz -g
ldflags := -Wl,-dead_strip

build: $(helpers)

test: build
	@build/test-symbol-fetcher
	@build/test-symbol-resolver
	@build/test-upload-listener
	@build/test-upload-receiver

build/%.dylib: %.c $(sources)
	@mkdir -p $(@D)
	$(ios_cc) \
		-isysroot $(shell xcrun --sdk iphoneos --show-sdk-path) \
		-arch arm64e \
		$(cflags) \
		-fno-ptrauth-calls \
		-fno-ptrauth-returns \
		-fno-stack-check \
		-fno-stack-protector \
		$< \
		-o $@ \
		-dynamiclib \
		$(ldflags)
	./serialize.py $@ ../injector.vala

build/test-%: %.c $(sources)
	@mkdir -p $(@D)
	$(macos_cc) \
		-isysroot $(shell xcrun --sdk macosx --show-sdk-path) \
		-arch arm64e \
		$(cflags) \
		-fno-ptrauth-calls \
		-fno-ptrauth-returns \
		-fno-stack-check \
		-fno-stack-protector \
		-DBUILDING_TEST_PROGRAM \
		$< \
		-o $@ \
		$(ldflags)

.PHONY: build test
