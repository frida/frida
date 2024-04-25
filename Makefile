PYTHON ?= $(shell which python3 >/dev/null && echo python3 || echo python)

all $(MAKECMDGOALS):
	@$(PYTHON) \
		-c "import sys; sys.path.insert(0, sys.argv[1]); from releng.meson_make import main; main()" \
		"$(shell pwd)" \
		./build \
		$(MAKECMDGOALS)

git-submodules:
	@[ ! -f releng/meson/meson.py ] && $(PYTHON) tools/ensure-submodules.py
-include git-submodules

.PHONY: all $(MAKECMDGOALS)
