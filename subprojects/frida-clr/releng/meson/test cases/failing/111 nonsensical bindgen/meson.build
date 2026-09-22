# SPDX-license-identifer: Apache-2.0
# Copyright © 2021 Intel Corporation

project('rustmod bindgen', 'c')

if not add_languages('rust', required: false)
  error('MESON_SKIP_TEST test requires rust compiler')
endif

prog_bindgen = find_program('bindgen', required : false)
if not prog_bindgen.found()
  error('MESON_SKIP_TEST bindgen not found')
endif

c_lib = static_library('clib', 'src/source.c')

import('unstable-rust').bindgen(
  input : c_lib,
  output : 'header.rs',
)
