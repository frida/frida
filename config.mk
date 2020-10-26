DESTDIR ?=
PREFIX ?= /usr

FRIDA := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

FRIDA_ASAN ?= no

ifeq ($(FRIDA_ASAN), yes)
FRIDA_MESONFLAGS_COMMON := -Doptimization=1 -Db_sanitize=address
FRIDA_MESONFLAGS_BOTTLE := -Doptimization=1 -Db_sanitize=address
FRIDA_ACOPTFLAGS_COMMON ?= -O1
FRIDA_ACOPTFLAGS_BOTTLE ?= -O1
else
FRIDA_MESONFLAGS_COMMON := -Doptimization=s -Db_ndebug=true --strip
FRIDA_MESONFLAGS_BOTTLE := -Doptimization=s -Db_ndebug=true
FRIDA_ACOPTFLAGS_COMMON ?= -Os
FRIDA_ACOPTFLAGS_BOTTLE ?= -Os
endif
FRIDA_ACDBGFLAGS_COMMON ?= -g3
FRIDA_ACDBGFLAGS_BOTTLE ?= -g1

FRIDA_V8_FLAGS := -Dv8=disabled
FRIDA_MAPPER_FLAGS := -Dmapper=auto

PYTHON ?= $(shell which python)
PYTHON_VERSION := $(shell $(PYTHON) -c 'import sys; v = sys.version_info; print("{0}.{1}".format(v[0], v[1]))')
PYTHON_NAME ?= python$(PYTHON_VERSION)
PYTHON_INCDIR ?=

PYTHON3 ?= python3

NODE ?= $(shell which node)
NODE_BIN_DIR := $(shell dirname $(NODE) 2>/dev/null)
NPM ?= $(NODE_BIN_DIR)/npm

MESON ?= $(PYTHON3) $(FRIDA)/releng/meson/meson.py
NINJA ?= $(FRIDA)/releng/ninja-$(build_platform_arch)

tests ?=
