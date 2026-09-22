MACOS_MINVER = 10.9
MACOS_CC := $(shell xcrun --sdk macosx -f clang)
MACOS_CFLAGS := -Wall -Oz -isysroot $(shell xcrun --sdk macosx --show-sdk-path) -mmacosx-version-min=$(MACOS_MINVER)
MACOS_LDFLAGS := -Wl,-dead_strip

IOS_MINVER = 7.0
IOS_CC := $(shell xcrun --sdk iphoneos -f clang)
IOS_CFLAGS := -Wall -Oz -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path) -miphoneos-version-min=$(IOS_MINVER)
IOS_LDFLAGS := -Wl,-dead_strip

WATCHOS_MINVER = 9.0
WATCHOS_CC := $(shell xcrun --sdk watchos -f clang)
WATCHOS_CFLAGS := -Wall -Oz -target arm64-apple-watchos$(WATCHOS_MINVER) -isysroot $(shell xcrun --sdk watchos --show-sdk-path)
WATCHOS_LDFLAGS := -Wl,-dead_strip

TVOS_MINVER = 13.0
TVOS_CC := $(shell xcrun --sdk appletvos -f clang)
TVOS_CFLAGS := -Wall -Oz -target arm64-apple-tvos$(TVOS_MINVER) -isysroot $(shell xcrun --sdk appletvos --show-sdk-path) -DHAVE_TVOS=1
TVOS_LDFLAGS := -Wl,-dead_strip

all: \
	sleeper-macos \
	sleeper-ios \
	sleeper-watchos \
	sleeper-tvos \
	stdio-writer-macos \
	stdio-writer-ios \
	stdio-writer-watchos \
	stdio-writer-tvos \
	forker-macos \
	forker-ios \
	forker-tvos \
	spawner-macos \
	spawner-ios \
	spawner-tvos \
	exception-catcher-macos \
	exception-catcher-ios \
	simple-agent-macos.dylib \
	simple-agent-ios.dylib \
	simple-agent-watchos.dylib \
	simple-agent-tvos.dylib \
	resident-agent-macos.dylib \
	resident-agent-ios.dylib \
	resident-agent-watchos.dylib \
	resident-agent-tvos.dylib

define declare-executable-macos
$1-macos: $2
	$$(MACOS_CC) $$(MACOS_CFLAGS) $$(MACOS_LDFLAGS) -framework CoreFoundation -arch arm64 $$< -o $$@.unsigned
	strip -Sx $$@.unsigned
	codesign -s "$$$$MACOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-executable-ios
$1-ios: $2
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch armv7 $$< -o $$@.armv7
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch arm64 $$< -o $$@.arm64
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch arm64e $$< -o $$@.arm64e
	strip -Sx $$@.armv7 $$@.arm64 $$@.arm64e
	lipo $$@.armv7 $$@.arm64 $$@.arm64e -create -output $$@.unsigned
	$(RM) $$@.arm64e
	codesign -s "$$$$IOS_CERTID" $$@.armv7
	mv $$@.armv7 $$@32
	codesign -s "$$$$IOS_CERTID" $$@.arm64
	mv $$@.arm64 $$@64
	codesign -s "$$$$IOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-executable-macos-foundation
$1-macos: $2
	$$(MACOS_CC) $$(MACOS_CFLAGS) $$(MACOS_LDFLAGS) -framework Foundation -arch arm64 $$< -o $$@.arm64
	$$(MACOS_CC) $$(MACOS_CFLAGS) $$(MACOS_LDFLAGS) -framework Foundation -arch x86_64 $$< -o $$@.x86_64
	strip -Sx $$@.arm64 $$@.x86_64
	lipo $$@.arm64 $$@.x86_64 -create -output $$@.unsigned
	$(RM) $$@.arm64
	$(RM) $$@.x86_64
	codesign -s "$$$$MACOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-executable-ios-foundation
$1-ios: $2
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -framework Foundation -arch armv7 $$< -o $$@.armv7
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -framework Foundation -arch arm64 $$< -o $$@.arm64
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -framework Foundation -arch arm64e $$< -o $$@.arm64e
	strip -Sx $$@.armv7 $$@.arm64 $$@.arm64e
	lipo $$@.armv7 $$@.arm64 $$@.arm64e -create -output $$@.unsigned
	$(RM) $$@.arm64e
	codesign -s "$$$$IOS_CERTID" $$@.armv7
	mv $$@.armv7 $$@32
	codesign -s "$$$$IOS_CERTID" $$@.arm64
	mv $$@.arm64 $$@64
	codesign -s "$$$$IOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-executable-watchos
$1-watchos: $2
	$$(WATCHOS_CC) $$(WATCHOS_CFLAGS) $$(WATCHOS_LDFLAGS) $$< -o $$@.tmp
	strip -Sx $$@.tmp
	codesign -s "$$$$WATCHOS_CERTID" $$@.tmp
	mv $$@.tmp $$@
endef

define declare-executable-tvos
$1-tvos: $2
	$$(TVOS_CC) $$(TVOS_CFLAGS) $$(TVOS_LDFLAGS) $$< -o $$@.tmp
	strip -Sx $$@.tmp
	codesign -s "$$$$TVOS_CERTID" $$@.tmp
	mv $$@.tmp $$@
endef

define declare-library-macos
$1-macos.dylib: $2
	$$(MACOS_CC) $$(MACOS_CFLAGS) $$(MACOS_LDFLAGS) -arch arm64 -dynamiclib $$< -o $$@.arm64
	$$(MACOS_CC) $$(MACOS_CFLAGS) $$(MACOS_LDFLAGS) -arch arm64e -dynamiclib $$< -o $$@.arm64e
	strip -Sx $$@.arm64 $$@.arm64e
	lipo $$@.arm64 $$@.arm64e -create -output $$@.unsigned
	$(RM) $$@.arm64 $$@.arm64e
	codesign -s "$$$$MACOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-library-ios
$1-ios.dylib: $2
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch armv7 -dynamiclib $$< -o $$@.armv7
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch arm64 -dynamiclib $$< -o $$@.arm64
	$$(IOS_CC) $$(IOS_CFLAGS) $$(IOS_LDFLAGS) -arch arm64e -dynamiclib $$< -o $$@.arm64e
	strip -Sx $$@.armv7 $$@.arm64 $$@.arm64e
	lipo $$@.armv7 $$@.arm64 $$@.arm64e -create -output $$@.unsigned
	$(RM) $$@.armv7 $$@.arm64 $$@.arm64e
	codesign -s "$$$$IOS_CERTID" $$@.unsigned
	mv $$@.unsigned $$@
endef

define declare-library-watchos
$1-watchos.dylib: $2
	$$(WATCHOS_CC) $$(WATCHOS_CFLAGS) $$(WATCHOS_LDFLAGS) -arch arm64 -dynamiclib $$< -o $$@.tmp
	strip -Sx $$@.tmp
	codesign -s "$$$$WATCHOS_CERTID" $$@.tmp
	mv $$@.tmp $$@
endef

define declare-library-tvos
$1-tvos.dylib: $2
	$$(TVOS_CC) $$(TVOS_CFLAGS) $$(TVOS_LDFLAGS) -arch arm64 -dynamiclib $$< -o $$@.tmp
	strip -Sx $$@.tmp
	codesign -s "$$$$TVOS_CERTID" $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable-macos,sleeper,sleeper-unix.c))
$(eval $(call declare-executable-ios,sleeper,sleeper-unix.c))
$(eval $(call declare-executable-watchos,sleeper,sleeper-unix.c))
$(eval $(call declare-executable-tvos,sleeper,sleeper-unix.c))

$(eval $(call declare-executable-macos,stdio-writer,stdio-writer.c))
$(eval $(call declare-executable-ios,stdio-writer,stdio-writer.c))
$(eval $(call declare-executable-watchos,stdio-writer,stdio-writer.c))
$(eval $(call declare-executable-tvos,stdio-writer,stdio-writer.c))

$(eval $(call declare-executable-macos,forker,forker.c))
$(eval $(call declare-executable-ios,forker,forker.c))
$(eval $(call declare-executable-tvos,forker,forker.c))

$(eval $(call declare-executable-macos,spawner,spawner-unix.c))
$(eval $(call declare-executable-ios,spawner,spawner-unix.c))
$(eval $(call declare-executable-tvos,spawner,spawner-unix.c))

$(eval $(call declare-executable-macos-foundation,exception-catcher,exception-catcher.m))
$(eval $(call declare-executable-ios-foundation,exception-catcher,exception-catcher.m))

$(eval $(call declare-library-macos,simple-agent,simple-agent.c))
$(eval $(call declare-library-ios,simple-agent,simple-agent.c))
$(eval $(call declare-library-watchos,simple-agent,simple-agent.c))
$(eval $(call declare-library-tvos,simple-agent,simple-agent.c))

$(eval $(call declare-library-macos,resident-agent,resident-agent.c))
$(eval $(call declare-library-ios,resident-agent,resident-agent.c))
$(eval $(call declare-library-watchos,resident-agent,resident-agent.c))
$(eval $(call declare-library-tvos,resident-agent,resident-agent.c))

.PHONY: all
