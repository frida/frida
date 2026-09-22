# Copyright © 2017-2018 Intel Corporation
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

project('gcc func attributes', ['c', 'cpp'])

# For msvc these will fail because msvc doesn't support __attribute__, for
# Clang and GCC, they should pass.
c = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if c.get_id() == 'pgi'
  error('MESON_SKIP_TEST: PGI supports its own set of features, will need a separate list for PGI to test it.')
endif

expected = {}
expected_default = not ['msvc', 'clang-cl', 'intel-cl'].contains(c.get_id())

# Q: Why is ifunc not in this list or any of the below lists?
# A: It's too damn hard to figure out if you actually support it, since it
#    requires both compiler and libc support, and there isn't a good way to
#    figure that out except by running the code we're trying to test.
attributes = [
  'aligned',
  'always_inline',
  'cold',
  'const',
  'constructor',
  'constructor_priority',
  'deprecated',
  'destructor',
  'flatten',
  'format',
  'format_arg',
  'gnu_inline',
  'hot',
  'malloc',
  'noinline',
  'nonnull',
  'noreturn',
  'nothrow',
  'pure',
  'section',
  'sentinel',
  'unused',
  'used',
  'vector_size',
  'warn_unused_result',
  'weak',
  'dllexport',
  'dllimport',
]

expected += {
  'dllexport': ['windows', 'cygwin'].contains(host_machine.system()),
  'dllimport': ['windows', 'cygwin'].contains(host_machine.system()),
}

if c.get_id() == 'gcc' and ['windows', 'cygwin'].contains(host_machine.system())
  expected += {
    'visibility': false,
    'visibility:hidden': false,
    'visibility:internal': false,
    'visibility:protected': false,
  }
endif

if c.get_id() != 'intel'
  # not supported by icc as of 19.0.0
  attributes += 'weakref'
endif

# These are unsupported on darwin with apple clang 9.1.0
if host_machine.system() != 'darwin'
  attributes += 'alias'
  attributes += 'visibility'
  attributes += 'visibility:default'
  attributes += 'visibility:hidden'
  attributes += 'visibility:internal'
  attributes += 'visibility:protected'
  attributes += 'alloc_size'
endif

# gcc doesn't support constructor_priority on darwin
if c.get_id() == 'gcc' and host_machine.system() == 'darwin'
  expected += {'constructor_priority': false}
endif

if ['gcc', 'intel'].contains(c.get_id())
  # not supported by clang as of 5.0.0 (at least up to 6.0.1)
  attributes += 'artificial'
  attributes += 'error'
  attributes += 'externally_visible'
  attributes += 'leaf'
  attributes += 'noclone'
  attributes += 'optimize'
  attributes += 'warning'

  if c.get_id() == 'gcc' and c.version().version_compare('>= 7.0.0')
    attributes += 'fallthrough'
  endif

  # XXX(arsen): limited to clang 13+ even though gcc 11 has it, since gcc
  # detects support for it at compile time based on binutils version
  if c.get_id() == 'clang' and c.version().version_compare('>= 13.0.0')
    attributes += 'retain'
  endif
endif

if get_option('mode') == 'single'
  foreach a : attributes
    x = c.has_function_attribute(a)
    expected_result = expected.get(a, expected_default)
    assert(x == expected_result, '@0@: @1@'.format(c.get_id(), a))
    x = cpp.has_function_attribute(a)
    assert(x == expected_result, '@0@: @1@'.format(cpp.get_id(), a))
  endforeach
else
  multi_expected = []
  foreach a : attributes
    if expected.get(a, expected_default)
      multi_expected += a
    endif
  endforeach

  multi_check = c.get_supported_function_attributes(attributes)
  assert(multi_check == multi_expected, 'get_supported_function_arguments works (C)')
  multi_check = cpp.get_supported_function_attributes(attributes)
  assert(multi_check == multi_expected, 'get_supported_function_arguments works (C++)')
endif
