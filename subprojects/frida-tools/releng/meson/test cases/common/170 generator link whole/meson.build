project('generator link_whole', 'c')

if meson.backend() == 'xcode'
    error('MESON_SKIP_TEST: whole-archive not supported in Xcode. Patches welcome.')
endif

# This just generates foo.h and foo.c with int foo() defined.
gen_py = find_program('generator.py')
gen = generator(gen_py,
  output: ['@BASENAME@.h', '@BASENAME@.c'],
  arguments : ['@INPUT@', '@BUILD_DIR@'])

# Test 1: link directly into executable
srcs = gen.process('meson_test_function.tmpl')
exe = executable('exe1', [srcs, 'main.c'], c_args : '-DBUILDING_EMBEDDED')
test('test1', exe)

# Test 2: link into shared library and access from executable
srcs = gen.process('meson_test_function.tmpl')
shlib2 = shared_library('shlib2', [srcs], c_args : '-DBUILDING_DLL')
exe = executable('exe2', 'main.c',
  link_with : shlib2,
  include_directories : shlib2.private_dir_include(),
)
test('test2', exe)

# Test 3: link into static library and access from executable
srcs = gen.process('meson_test_function.tmpl')
stlib3 = static_library('stlib3', [srcs], c_args : '-DBUILDING_EMBEDDED')
exe = executable('exe3', 'main.c',
  c_args : '-DBUILDING_EMBEDDED',
  link_with : stlib3,
  include_directories : stlib3.private_dir_include(),
)
test('test3', exe)

# Test 4: link into static library, link into shared
# and access from executable. To make sure static_library
# is not dropped use pull_meson_test_function helper.
srcs = gen.process('meson_test_function.tmpl')
stlib4 = static_library('stlib4', [srcs], c_args : '-DBUILDING_DLL')
shlib4 = shared_library('shlib4', 'pull_meson_test_function.c',
  c_args : '-DBUILDING_DLL',
  link_with : stlib4,
  include_directories : stlib4.private_dir_include(),
)
exe = executable('exe4', 'main.c',
  link_with : shlib4,
  include_directories : stlib4.private_dir_include(),
)
test('test4', exe)

# Test 5: link into static library, link_whole into shared
# and access from executable
srcs = gen.process('meson_test_function.tmpl')
stlib5 = static_library('stlib5', [srcs], c_args : '-DBUILDING_DLL')
shlib5 = shared_library('shlib5', link_whole : stlib5)
exe = executable('exe5', 'main.c',
  link_with : shlib5,
  include_directories : stlib5.private_dir_include(),
)
test('test5', exe)
