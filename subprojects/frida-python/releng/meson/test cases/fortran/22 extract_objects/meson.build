project('test_project', 'fortran')

if get_option('unity') == 'on'
  error('MESON_SKIP_TEST: extract_objects does not work in unity builds')
endif

libfoo = static_library(
  'foo',
  sources : ['foo1.f90', 'foo2.f90'])

foo1_object = libfoo.extract_objects('foo1.f90')

libfinal = library(
  'final',
  sources : 'bar.f90',
  objects : foo1_object,
)
