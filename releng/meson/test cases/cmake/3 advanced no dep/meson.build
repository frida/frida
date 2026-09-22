project('cmakeSubTest_advanced', ['c', 'cpp'])

cm = import('cmake')

# Test the "normal" subproject call
sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib')
sub_sta = sub_pro.dependency('cmModLibStatic')

# Build some files
exe1 = executable('main1', ['main.cpp'], dependencies: [sub_dep])
exe2 = executable('main2', ['main.cpp'], dependencies: [sub_sta])
test('test1', exe1)
test('test2', exe2)

# Test if we can also extract executables
assert(sub_pro.target_type('meson-testEXE') == 'executable', 'The type must be executable for obvious reasons')
test('test3', sub_pro.target('meson-testEXE'))
test('test4', sub_pro.target('benchmark'))
