project('sdl2 test', 'c')

method = get_option('method')

sdl2_dep = dependency('sdl2', version : '>=2.0.0', required : false, method : method)

if not sdl2_dep.found()
  error('MESON_SKIP_TEST sdl2 not found.')
endif

e = executable('sdl2prog', 'sdl2prog.c', dependencies : sdl2_dep)

test('sdl2test', e)
