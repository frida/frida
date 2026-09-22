project('rlib linkage', 'rust', default_options : ['rust_std=2018'])

lib = static_library(
  'lib',
  'lib.rs',
  rust_crate_type : 'rlib',
)

lib2 = static_library(
  'lib2',
  'lib2.rs',
  rust_crate_type : 'rlib',
  link_with : lib,
)

exe = executable(
  'exe',
  'main.rs',
  link_with : lib2,
)

test('main', exe)
