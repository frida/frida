# SPDX-License-Identifier: Apache-2.0
# Copyright © 2022 Intel Corporation

dep_zlib = dependency('zlib', required : false, disabler : true)

external_dep_rs = rust.bindgen(
  input : 'external_dep.h',
  output : 'external_dep.rs',
  dependencies : dep_zlib
)

external_dep = static_library(
  'external_dep',
  [external_dep_rs],
  # for generated code, do not lint
  rust_args: ['-A', 'warnings'],
  dependencies : dep_zlib.partial_dependency(links : true),
)

rust.test('external dep', external_dep)

int_dep = declare_dependency(
  sources : [gen_h, gen2_h],
  include_directories : include_directories('..'),
)

internal_dep_rs = rust.bindgen(
  input : 'internal_dep.h',
  output : 'internal_dep.rs',
  dependencies : int_dep,
)

c_lib2 = static_library(
  'clib2',
  'clib2.c',
  dependencies : int_dep,
)

rust_bin_int_dep = executable(
  'rust_bin_int_dep',
  structured_sources(['internal_main.rs', internal_dep_rs]),
  link_with : [c_lib, c_lib2],
)

test('generated header dependency', rust_bin_int_dep)
