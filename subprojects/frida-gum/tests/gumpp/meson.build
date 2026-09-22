gumpp_sources = [
  'backtracer.cxx',
]

gum_tests_gumpp = static_library('gum-tests-gumpp', gumpp_sources,
  override_options: [
    'cpp_std=c++14',
    'cpp_eh=default',
    'cpp_rtti=false',
  ],
  include_directories: test_incdirs,
  dependencies: [gumpp_dep, gum_dep],
)
