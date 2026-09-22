CC := $$QNX_HOST/usr/bin/arm-unknown-nto-qnx6.5.0-gcc
STRIP := $$QNX_HOST/usr/bin/arm-unknown-nto-qnx6.5.0-strip
CFLAGS := \
	--sysroot=$$QNX_TARGET/armle \
	-no-canonical-prefixes \
	-Wall \
	-pipe \
	-fPIC \
	-Os \
	-march=armv6 -mfloat-abi=softfp -mfpu=vfpv3-d16 \
	-fdata-sections -ffunction-sections \
	-funwind-tables -fno-exceptions \
	-Dqnx \
	-I$$QNX_TARGET/usr/include
LDFLAGS := \
	-Wl,--fix-cortex-a8 \
	-Wl,--no-undefined \
	-Wl,-z,noexecstack \
	-Wl,-z,relro \
	-Wl,-z,now \
	-Wl,--gc-sections

all: \
	sleeper-qnx-arm \
	forker-qnx-arm \
	spawner-qnx-arm \
	simple-agent-qnx-arm.so \
	resident-agent-qnx-arm.so \
	$(NULL)

define declare-executable
$1-qnx-arm: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp
	$$(STRIP) --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c))

%-agent-qnx-arm.so: %-agent.c %-agent-qnx-arm.version
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-shared \
		-Wl,-soname,$*-agent-qnx-arm.so \
		-Wl,--version-script=$*-agent-qnx-arm.version \
		$< \
		-o $@.tmp
	$(STRIP) --strip-all $@.tmp
	mv $@.tmp $@

%-agent-qnx-arm.version:
	echo "{"     > $@.tmp
	echo "  global:"             >> $@.tmp
	echo "    frida_agent_main;" >> $@.tmp
	echo ""                      >> $@.tmp
	echo "  local:"              >> $@.tmp
	echo "    *;"                >> $@.tmp
	echo "};"                    >> $@.tmp
	mv $@.tmp $@

.PHONY: all
