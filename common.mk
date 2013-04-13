modules = udis86 frida-gum

git-submodules:
	@if [ ! -f udis86/configure.ac ]; then \
		git submodule init; \
		git submodule update; \
	fi
-include git-submodules

define make-update-submodule-stamp
$1-update-submodule-stamp: git-submodules
	cd $1 && git log -1 --format=%H > ../build/$1-submodule-stamp.tmp && git status >> ../build/$1-submodule-stamp.tmp
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

build/frida-env-%.rc:
	FRIDA_TARGET=$* ./setup-env.sh
