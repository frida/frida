project('cmMod', ['c', 'cpp'])

cm        = import('cmake')
cmOpts    = ['-DUSE_PTHREAD=@0@'.format(get_option('use_pthread'))]
cmMod     = cm.subproject('cmMod', cmake_options: cmOpts)
cmModDep1 = cmMod.dependency('cmModLib')
cmModDep2 = cmMod.dependency('cmModLib_shared')

exe1 = executable('exe1', ['main.cpp'], dependencies: [cmModDep1])
exe2 = executable('exe2', ['main.cpp'], dependencies: [cmModDep2])
test('exe1_OK', exe1)
test('exe2_OK', exe2)
