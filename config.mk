DESTDIR ?=
PREFIX ?= /usr

FRIDA := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

FRIDA_ASAN ?= no

ifeq ($(FRIDA_ASAN), yes)
FRIDA_COMMON_FLAGS := --buildtype debugoptimized -Db_sanitize=address
FRIDA_SDK_FLAGS := --buildtype debugoptimized -Db_sanitize=address
FRIDA_OPTIMIZATION_FLAGS ?= -O1
FRIDA_SDK_OPTIMIZATION_FLAGS ?= -O1
else
FRIDA_COMMON_FLAGS := --buildtype minsize --strip
FRIDA_SDK_FLAGS := --buildtype minsize
FRIDA_OPTIMIZATION_FLAGS ?= -Os
FRIDA_SDK_OPTIMIZATION_FLAGS ?= -Os
endif

FRIDA_MAPPER_FLAGS := -Dmapper=auto

FRIDA_DEBUG_FLAGS ?= -g3
FRIDA_SDK_DEBUG_FLAGS ?= -g1

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
