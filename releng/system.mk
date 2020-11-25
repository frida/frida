build_os := $(shell uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,macos,')
build_arch := $(shell releng/detect-arch.sh)
build_os_arch := $(build_os)-$(build_arch)

ifeq ($(build_os), $(filter $(build_os), macos ios))
sed_regex_option := -E
else
sed_regex_option := -r
endif
