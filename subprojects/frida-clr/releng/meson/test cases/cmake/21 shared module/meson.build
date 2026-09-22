project('cmakeSharedModule', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('myMod')

dl = meson.get_compiler('c').find_library('dl', required: false)

l = shared_library('runtime', 'runtime.c')
e = executable('prog', ['prog.c'], link_with: l, dependencies: [sub_dep, dl])
m = sub_pro.target('myMod')
test('test1', e, args : m)
