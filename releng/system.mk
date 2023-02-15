build_os := $(shell releng/detect-os.sh)
build_arch := $(shell releng/detect-arch.sh)
build_variant := $(shell releng/detect-variant.sh)
build_os_arch := $(build_os)-$(build_arch)
ifeq ($(build_variant),)
build_machine := $(build_os)-$(build_arch)
else
build_machine := $(build_os)-$(build_arch)-$(build_variant)
endif

ifeq ($(build_os), $(filter $(build_os), macos ios))
sed_regex_option := -E
else
sed_regex_option := -r
endif
