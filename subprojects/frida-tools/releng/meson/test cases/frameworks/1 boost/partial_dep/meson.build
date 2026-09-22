# Copyright © 2018 Intel Corporation
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

dep_boost = dependency('boost')
dep_boost_headers = dep_boost.partial_dependency(compile_args : true)

libfoo = static_library(
  'foo',
  'foo.cpp',
  dependencies : dep_boost_headers,
)

exe_external_dep = executable(
  'external_dep',
  'main.cpp',
  dependencies : dep_boost,
  link_with : libfoo
)

test('External Dependency', exe_external_dep)
