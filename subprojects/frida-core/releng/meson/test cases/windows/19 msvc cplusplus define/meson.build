project('msvc __cplusplus', 'cpp', default_options : ['cpp_std=c++14'])

cpp = meson.get_compiler('cpp')

if cpp.get_id() != 'msvc'
  error('MESON_SKIP_TEST: test is only relevant for msvc')
elif meson.project_version().version_compare('< 15.7')
  error('MESON_SKIP_TEST: test is only relevant for msvc versions >= 15.7')
endif

test(
  'main',
  executable('main', 'main.cpp'),
)
