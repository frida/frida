heap_sources = [
  'allocationtracker.c',
  'allocatorprobe.c',
  'allocatorprobex.cxx',
  'boundschecker.c',
  'cobjecttracker.c',
  'instancetracker.c',
  'pagepool.c',
  'sanitychecker.c',
]

gum_tests_heap = static_library('gum-tests-heap', heap_sources,
  include_directories: test_incdirs,
  dependencies: [gum_dep],
)
