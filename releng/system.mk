build_os := $(shell releng/detect-os.sh)
build_arch := $(shell releng/detect-arch.sh)
build_os_arch := $(build_os)-$(build_arch)
build_machine := $(build_os_arch)

ifeq ($(build_os), $(filter $(build_os), macos ios))
sed_regex_option := -E
else
sed_regex_option := -r
endif
