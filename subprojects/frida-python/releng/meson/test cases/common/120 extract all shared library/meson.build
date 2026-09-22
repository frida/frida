project('extract all', 'c', 'cpp')

if meson.backend() == 'xcode'
    error('MESON_SKIP_TEST: Xcode backend does not handle libraries with only objects, not sources.')
endif

a = static_library('a', 'one.c', 'two.c')
b = static_library('b', 'three.c', 'four.c')
# libc.so cannot be used, it already exists as a reserved name
c = shared_library('cee',
  objects : [a.extract_all_objects(), b.extract_all_objects()],
  vs_module_defs : 'func1234.def')

e = executable('proggie', 'prog.c', link_with : c)
test('extall', e)
