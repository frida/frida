project('Mercurial outchecker', 'c')

sp = subproject('samplesubproject')

exe = executable('gitprog', 'prog.c',
include_directories : sp.get_variable('subproj_inc'),
link_with : sp.get_variable('subproj_lib'),
)

test('maintest', exe)
