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

dec_sub_dep = declare_dependency(
  include_directories : include_directories('headers'),
)

dec_dep = declare_dependency(
  sources : files('headers/foo.c'),
  dependencies : dec_sub_dep,
)

sub_dep = dec_dep.partial_dependency(includes : true)

dec_exe = executable(
  'declare_dep',
  files('main.c', 'other.c'),
  dependencies : sub_dep,
)

test('Declare Dependency', dec_exe)
