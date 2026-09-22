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

project('config tool variable', 'cpp')


dep_llvm = dependency('llvm', method : 'config-tool', required : false)
if not dep_llvm.found()
  error('MESON_SKIP_TEST LLVM not installed.')
endif

includedir = dep_llvm.get_configtool_variable('includedir')
includedir = join_paths(includedir, 'llvm')
if host_machine.system() == 'windows'
  cmd = run_command(['dir', includedir], check: false)
else
  cmd = run_command(['ls', includedir], check: false)
endif

assert(cmd.returncode() == 0, 'did not run successfully')
