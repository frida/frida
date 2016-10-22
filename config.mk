BINDIST = build/bindist
DESTDIR ?=
PREFIX ?= /usr

FRIDA := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
FRIDA_OPTIMIZATION_FLAGS ?= -Os
FRIDA_DEBUG_FLAGS ?= -g3
FRIDA_STRIP ?= yes
FRIDA_DIET ?= auto
FRIDA_MAPPER ?= no
FRIDA_ASAN ?= no

PYTHON ?= $(shell which python)
PYTHON_VERSION := $(shell $(PYTHON) -c 'import sys; v = sys.version_info; print("{0}.{1}".format(v[0], v[1]))')
PYTHON_NAME ?= python$(PYTHON_VERSION)

NODE ?= $(shell which node)
NODE_BIN_DIR := $(shell dirname $(NODE) 2>/dev/null)
NPM ?= $(NODE_BIN_DIR)/npm

tests ?=
