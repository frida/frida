CC := clang
CFLAGS := -Wall -pipe -Os -fPIC -fdata-sections -ffunction-sections
LDFLAGS := -Wl,--gc-sections

arch := $(shell uname -m | sed 's,^amd64$$,x86_64,')

all: \
	sleeper-freebsd-$(arch) \
	forker-freebsd-$(arch) \
	spawner-freebsd-$(arch) \
	simple-agent-freebsd-$(arch).so \
	resident-agent-freebsd-$(arch).so \
	$(NULL)

define declare-executable
$1-freebsd-$$(arch): $2
	$$(CC) $$(CFLAGS) $$(LDFLAGS) $$< -o $$@.tmp $3
	strip --strip-all $$@.tmp
	mv $$@.tmp $$@
endef

$(eval $(call declare-executable,sleeper,sleeper-unix.c))

$(eval $(call declare-executable,forker,forker.c))

$(eval $(call declare-executable,spawner,spawner-unix.c,-ldl))

%-agent-freebsd-$(arch).so: %-agent.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared $< -o $@.tmp
	strip --strip-all $@.tmp
	mv $@.tmp $@

.PHONY: all
