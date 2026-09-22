gumjs_sources = [
  'script.c',
  'kscript.c',
]

gum_tests_gumjs = static_library('gum-tests-gumjs', gumjs_sources,
  include_directories: test_incdirs,
  dependencies: [gumjs_dep, gumjs_inspector_dep],
)
