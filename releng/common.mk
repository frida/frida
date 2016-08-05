FRIDA_VERSION := $(shell git describe --tags --always --long | sed 's,-,.,g' | cut -f1-3 -d'.')

modules = capstone frida-gum frida-core frida-python frida-node

git-submodules:
	@if [ ! -f frida-core/configure.ac ]; then \
		git submodule init; \
		git submodule update; \
	fi
-include git-submodules

define make-update-submodule-stamp
$1-update-submodule-stamp: git-submodules
	@mkdir -p build
	@cd $1 \
		&& git log -1 --format=%H > ../build/$1-submodule-stamp.tmp \
		&& git status >> ../build/$1-submodule-stamp.tmp \
		&& git diff >> ../build/$1-submodule-stamp.tmp
	@if [ -f build/$1-submodule-stamp ]; then \
		if cmp -s build/$1-submodule-stamp build/$1-submodule-stamp.tmp; then \
			rm build/$1-submodule-stamp.tmp; \
		else \
			mv build/$1-submodule-stamp.tmp build/$1-submodule-stamp; \
		fi \
	else \
		mv build/$1-submodule-stamp.tmp build/$1-submodule-stamp; \
	fi
endef
$(foreach m,$(modules),$(eval $(call make-update-submodule-stamp,$m)))
git-submodule-stamps: $(foreach m,$(modules),$m-update-submodule-stamp)
-include git-submodule-stamps

build/frida-env-%.rc: releng/setup-env.sh releng/config.site.in build/frida-version.h
	FRIDA_HOST=$* \
		FRIDA_OPTIMIZATION_FLAGS="$(FRIDA_OPTIMIZATION_FLAGS)" \
		FRIDA_DEBUG_FLAGS="$(FRIDA_DEBUG_FLAGS)" \
		FRIDA_STRIP=$(FRIDA_STRIP) \
		FRIDA_DIET=$(FRIDA_DIET) \
		FRIDA_MAPPER=$(FRIDA_MAPPER) \
		FRIDA_ASAN=$(FRIDA_ASAN) \
		./releng/setup-env.sh

build/frida-version.h: releng/generate-version-header.py .git/refs/heads/master
	@python releng/generate-version-header.py > $@.tmp
	@mv $@.tmp $@

ensure_relink = test $(1) -nt $(2) || touch -c $(2)
