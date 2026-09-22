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

prog_python = import('python3').find_python()

target = custom_target(
  'target',
  output : ['1', '2'],
  command : [prog_python, '-c',
             'with open("1", "w") as f: f.write("foo"); with open("2", "w") as f: f.write("foo")'],
)

target[0] = 'foo'
