ENV := ../../../build/frida-env-android-arm.rc

CC := $(shell source $(ENV) && echo $$CC)
STRIP := $(shell source $(ENV) && echo $$STRIP)
CFLAGS := -Wall -pipe -Os -mthumb $(shell source $(ENV) && echo $$CFLAGS)
LDFLAGS := -Wl,--no-undefined $(shell source $(ENV) && echo $$LDFLAGS)

all: \
	sleeper-android-arm \
	forker-android-arm \
	spawner-android-arm \
	simple-agent-android-arm.so \
	resident-agent-android-arm.so \
	$(NULL)

define declare-executable
$1-android-arm: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) -pie $$< -o $$@.tmp
	$$(STRIP) --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c))

%-agent-android-arm.so: %-agent.c %-agent-android-arm.version
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-shared \
		-Wl,-soname,$*-agent-android-arm.so \
		-Wl,--version-script=$*-agent-android-arm.version \
		$< \
		-o $@.tmp
	$(STRIP) --strip-all $@.tmp
	mv $@.tmp $@

%-agent-android-arm.version:
	echo "LABRAT_AGENT_ANDROID_ARM_1.0 {"  > $@.tmp
	echo "  global:"                      >> $@.tmp
	echo "    frida_agent_main;"          >> $@.tmp
	echo ""                               >> $@.tmp
	echo "  local:"                       >> $@.tmp
	echo "    *;"                         >> $@.tmp
	echo "};"                             >> $@.tmp
	mv $@.tmp $@

.PHONY: all
