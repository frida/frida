ENV := ../../../build/frida-env-android-arm64.rc

CC := $(shell source $(ENV) && echo $$CC)
STRIP := $(shell source $(ENV) && echo $$STRIP)
CFLAGS := -Wall -pipe -Os $(shell source $(ENV) && echo $$CFLAGS)
LDFLAGS := -Wl,--no-undefined $(shell source $(ENV) && echo $$LDFLAGS)

all: \
	sleeper-android-arm64 \
	forker-android-arm64 \
	spawner-android-arm64 \
	simple-agent-android-arm64.so \
	resident-agent-android-arm64.so \
	$(NULL)

define declare-executable
$1-android-arm64: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) -pie $$< -o $$@.tmp
	$$(STRIP) --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c))

%-agent-android-arm64.so: %-agent.c %-agent-android-arm64.version
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-shared \
		-Wl,-soname,$*-agent-android-arm64.so \
		-Wl,--version-script=$*-agent-android-arm64.version \
		$< \
		-o $@.tmp
	$(STRIP) --strip-all $@.tmp
	mv $@.tmp $@

%-agent-android-arm64.version:
	echo "LABRAT_AGENT_ANDROID_ARM64_1.0 {" > $@.tmp
	echo "  global:"             >> $@.tmp
	echo "    frida_agent_main;" >> $@.tmp
	echo ""                      >> $@.tmp
	echo "  local:"              >> $@.tmp
	echo "    *;"                >> $@.tmp
	echo "};"                    >> $@.tmp
	mv $@.tmp $@

.PHONY: all
