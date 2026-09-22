# SPDX-license-identifer: Apache-2.0
# Copyright © 2021-2023 Intel Corporation

fs = import('fs')

cpp_header = fs.copyfile('../src/header.hpp', 'cpp_header.h')

cpp_bind_override = rust.bindgen(
  input : cpp_header,
  output : 'generated-cpp.rs',
  language : 'cpp',
)

cpp_exe2 = executable(
  'cpp_exe2',
  structured_sources(['../src/cpp.rs', cpp_bind_override]),
  link_with : cpp_lib,
)
test('cpp-forced', cpp_exe2)
