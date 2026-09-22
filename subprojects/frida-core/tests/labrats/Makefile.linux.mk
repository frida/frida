CC := gcc
CFLAGS := -Wall -pipe -Os -fPIC -fdata-sections -ffunction-sections
LDFLAGS := -Wl,--gc-sections
ARCHS := \
	linux-x86 \
	linux-x86_64 \
	linux-arm \
	linux-armbe8 \
	linux-armhf \
	linux-arm64 \
	linux-arm64be \
	linux-arm64beilp32 \
	linux-mips \
	linux-mipsel \
	linux-mips64 \
	linux-mips64el

all: $(ARCHS) $(NULL)

define declare-arch
$1: resident-agent-$1.so simple-agent-$1.so sleeper-$1 forker-$1 spawner-$1
endef

$(foreach arch,$(ARCHS),$(eval $(call declare-arch,$(arch))))

define declare-executable
$1-linux-x86: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) -m32 $$< -o $$@.tmp $3
	strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-x86_64: $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) -m64 $$< -o $$@.tmp $3
	strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-arm: $2
	arm-linux-gnueabi-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	arm-linux-gnueabi-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-armbe8: $2
	armeb-linux-gnueabi-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	armeb-linux-gnueabi-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-armhf: $2
	arm-linux-gnueabihf-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	arm-linux-gnueabihf-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-arm64: $2
	aarch64-linux-gnu-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	aarch64-linux-gnu-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-arm64be: $2
	aarch64_be-linux-gnu-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	aarch64_be-linux-gnu-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-arm64beilp32: $2
	aarch64_be-linux-gnu_ilp32-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	aarch64_be-linux-gnu_ilp32-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-mips: $2
	mips-linux-gnu-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	mips-linux-gnu-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-mipsel: $2
	mipsel-linux-gnu-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	mipsel-linux-gnu-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-mips64: $2
	mips64-linux-gnuabi64-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	mips64-linux-gnuabi64-strip --strip-all $$@.tmp
	mv $$@.tmp $$@

$1-linux-mips64el: $2
	mips64el-linux-gnuabi64-gcc $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	mips64el-linux-gnuabi64-strip --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c,-ldl))

%-agent-linux-x86.so: %-agent.c
	$(CC) $(CFLAGS) $(LDFLAGS) -m32 -shared $< -o $@.tmp
	strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-x86_64.so: %-agent.c
	$(CC) $(CFLAGS) $(LDFLAGS) -m64 -shared $< -o $@.tmp
	strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-arm.so: %-agent.c
	arm-linux-gnueabi-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	arm-linux-gnueabi-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-armbe8.so: %-agent.c
	armeb-linux-gnueabi-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	armeb-linux-gnueabi-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-armhf.so: %-agent.c
	arm-linux-gnueabihf-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	arm-linux-gnueabihf-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-arm64.so: %-agent.c
	aarch64-linux-gnu-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	aarch64-linux-gnu-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-arm64be.so: %-agent.c
	aarch64_be-linux-gnu-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	aarch64_be-linux-gnu-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-arm64beilp32.so: %-agent.c
	aarch64_be-linux-gnu_ilp32-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	aarch64_be-linux-gnu_ilp32-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-mips.so: %-agent.c
	mips-linux-gnu-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	mips-linux-gnu-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-mipsel.so: %-agent.c
	mipsel-linux-gnu-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	mipsel-linux-gnu-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-mips64.so: %-agent.c
	mips64-linux-gnuabi64-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	mips64-linux-gnuabi64-strip --strip-all $@.tmp
	mv $@.tmp $@

%-agent-linux-mips64el.so: %-agent.c
	mips64el-linux-gnuabi64-gcc $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	mips64el-linux-gnuabi64-strip --strip-all $@.tmp
	mv $@.tmp $@

.PHONY: all
