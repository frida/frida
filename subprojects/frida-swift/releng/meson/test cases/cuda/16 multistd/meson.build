project('C++-CUDA multi-std', 'cpp', 'cuda',
        version : '1.0.0',
        default_options : ['cpp_std=c++17', 'cuda_std=c++14'])

# Regression test: Passing override_options used to cause a crash.
# See https://github.com/mesonbuild/meson/issues/9448.
libcpp11 = static_library('testcpp11', 'lib.cu',
  override_options: ['cpp_std=c++11']
)

exe = executable('prog', 'main.cu', link_with: libcpp11)
# The runtimes leak memory, so ignore it.
test('cudatest', exe, env: ['ASAN_OPTIONS=detect_leaks=0'])
