ENV := ../../../build/frida-env-android-x86.rc

CC := $(shell source $(ENV) && echo $$CC)
STRIP := $(shell source $(ENV) && echo $$STRIP)
CFLAGS := -Wall -pipe -Os $(shell source $(ENV) && echo $$CFLAGS)
LDFLAGS := -Wl,--no-undefined $(shell source $(ENV) && echo $$LDFLAGS)

all: \
	sleeper-android-x86 \
	forker-android-x86 \
	spawner-android-x86 \
	simple-agent-android-x86.so \
	resident-agent-android-x86.so \
	$(NULL)

define declare-executable
$1-android-x86: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) -pie $$< -o $$@.tmp
	$$(STRIP) --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c))

%-agent-android-x86.so: %-agent.c %-agent-android-x86.version
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-shared \
		-Wl,-soname,$*-agent-android-x86.so \
		-Wl,--version-script=$*-agent-android-x86.version \
		$< \
		-o $@.tmp
	$(STRIP) --strip-all $@.tmp
	mv $@.tmp $@

%-agent-android-x86.version:
	echo "LABRAT_AGENT_ANDROID_X86_1.0 {"   > $@.tmp
	echo "  global:"                       >> $@.tmp
	echo "    frida_agent_main;"           >> $@.tmp
	echo ""                                >> $@.tmp
	echo "  local:"                        >> $@.tmp
	echo "    *;"                          >> $@.tmp
	echo "};"                              >> $@.tmp
	mv $@.tmp $@

.PHONY: all
