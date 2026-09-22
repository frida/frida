KERNEL_TREE ?= $(HOME)/src/linux

archs := \
	x86 \
	x86_64 \
	arm \
	arm64 \
	mips \
	mips64 \
	s390x \
	$(NULL)

all: $(foreach arch,$(archs),$(arch).vala)

clean:
	$(RM) $(foreach arch,$(archs),$(arch).vala $(arch)-glue.c)

%.vala %-glue.c:
	./generate-bindings.py \
		--abi=$* \
		--kernel-tree=$(KERNEL_TREE) \
		--out-vala=$*.vala \
		--out-c=$*-glue.c

.PHONY: all clean
