# Copyright © 2017 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project('custom_target_index', 'c', default_options : 'c_std=c89')

py_mod = import('python3')
prog_python = py_mod.find_python()

gen = custom_target(
  'gen.[ch]',
  input : 'gen_sources.py',
  output : ['gen.c', 'gen.h'],
  command : [prog_python, '@INPUT@', '--header', '@OUTPUT1@', '--code', '@OUTPUT0@'],
)

has_not_changed = false
if is_disabler(gen)
    has_not_changed = true
else
    has_not_changed = true
endif

assert(has_not_changed, 'Custom target has changed.')

assert(not is_disabler(gen), 'Custom target is a disabler.')

lib = static_library(
  'libfoo',
  ['lib.c', gen[1]],
)

has_not_changed = false
if is_disabler(lib)
    has_not_changed = true
else
    has_not_changed = true
endif

assert(has_not_changed, 'Static library has changed.')

assert(not is_disabler(lib), 'Static library is a disabler.')

custom_target(
  'foo',
  input: gen[0],
  output: 'foo',
  command: [find_program('check_args.py'), '@INPUT@'],
)

subdir('subdir')

gen = disabler()

assert(is_disabler(gen), 'Generator is not a disabler.')

lib = static_library(
  'libfoo',
  ['lib.c', gen[1]],
)

assert(is_disabler(lib), 'Static library is not a disabler.')

if lib.found()
  lib_disabled = false
else
  lib_disabled = true
endif

assert(lib_disabled, 'Static library was not disabled.')
