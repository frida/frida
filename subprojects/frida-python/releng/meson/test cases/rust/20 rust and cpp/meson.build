# SPDX-License-Identifier: Apache-2.0
# Copyright © 2023 Intel Corporation

project(
  'Rust and C++',
  'rust', 'cpp',
  default_options : ['cpp_std=c++14'],
  meson_version : '>= 1.2.0',
)

cpplib = static_library('cpp', 'lib.cpp')
exe = executable('main', 'main.rs', link_with : cpplib)

test('main', exe)
