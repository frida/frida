project('cmakeSubTest_advanced', ['c', 'cpp'], meson_version : '>= 1.3.0')

dep_test = dependency('ZLIB', method: 'cmake', required: false)
if not dep_test.found()
  error('MESON_SKIP_TEST: zlib is not installed')
endif

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
assert(sub_pro.target_type('testEXE') == 'executable', 'The type must be executable for obvious reasons')
test('test3', sub_pro.target('testEXE'))

# Test that we can add a new target with the same name as the CMake subproject
exe4 = executable('testEXE', ['main.cpp'], dependencies: [sub_sta])
test('test4', exe4)

# Test the "normal" subproject call for the build machine
sub_pro_b = cm.subproject('cmMod', native : true)
sub_dep_b = sub_pro_b.dependency('cmModLib')
sub_sta_b = sub_pro_b.dependency('cmModLibStatic')

# Build some files
exe1_b = executable('main1_b', ['main.cpp'], dependencies: [sub_dep_b], native : true)
exe2_b = executable('main2_b', ['main.cpp'], dependencies: [sub_sta_b], native : true)
