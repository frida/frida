top_srcdir := $(shell cd ../../.. && pwd)
releng := $(top_srcdir)/releng
data_dir := $(top_srcdir)/tests/data

BUILDDIR ?= $(top_srcdir)/build
MESON ?= $(releng)/meson/meson.py

ifdef FRIDA_HOST
host_machine := $(FRIDA_HOST)
else
host_machine := $(shell basename $$(ls -1 $(BUILDDIR)/frida-linux-*.txt | tail -1) | cut -d"." -f1 | cut -d"-" -f2-)
endif
host_arch := $(shell echo $(host_machine) | cut -d"-" -f2)

crossfile := $(BUILDDIR)/frida-$(host_machine).txt
pkg_config_path := $(top_srcdir)/deps/sdk-$(host_machine)/lib/pkgconfig

build:
	$(MESON) setup \
		--cross-file $(crossfile) \
		--libdir "$(data_dir)" \
		--strip \
		-Dstem_suffix="-$(host_machine)" \
		-Dpkg_config_path="$(pkg_config_path)" \
		--wipe \
		build
	$(MESON) install -C build

.PHONY: build
