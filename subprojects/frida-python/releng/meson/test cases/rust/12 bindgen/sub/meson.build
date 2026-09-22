gen_rs3 = rust.bindgen(
  input : [gen_h, gen2_h],
  output : 'gen.rs',
)

f3 = configure_file(
  input : '../src/main2.rs',
  output : 'main3.rs',
  copy : true,
)

rust_bin3 = executable(
  'rust_bin3',
  [f3, gen_rs3],
  link_with : c_lib,
)

test('generated header (subdir)', rust_bin3)

gen4 = rust.bindgen(
  input : '../src/header.h',
  output : 'header.rs',
  include_directories : inc,
)

# see: https://github.com/mesonbuild/meson/issues/8160
f4 = configure_file(
  input : '../src/main.rs',
  output : 'main.rs',
  copy : true,
)

rust_bin4 = executable(
  'rust_bin_subdir',
  [f4, gen4],
  link_with : c_lib,
)

test('static header (subdir)', rust_bin4)
