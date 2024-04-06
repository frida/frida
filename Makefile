PYTHON ?= $(shell which python3 >/dev/null && echo python3 || echo python)

all $(MAKECMDGOALS):
	@$(PYTHON) \
		-c "import sys; sys.path.insert(0, sys.argv[1]); from releng.meson_make import main; main()" \
		"$(shell pwd)" \
		./build \
		$(MAKECMDGOALS)

git-submodules:
	@set -e; if [ ! -f releng/meson/meson.py ]; then \
		git submodule update --init --depth 1 subprojects/frida-core subprojects/frida-gum; \
		git submodule update --init --depth 1 --recursive releng; \
	fi
-include git-submodules

.PHONY: all $(MAKECMDGOALS)
