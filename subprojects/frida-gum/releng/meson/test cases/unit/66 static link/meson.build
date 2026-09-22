project('test static link', 'c')

cc = meson.get_compiler('c')

# Verify that installed libfunc2.a is usable
func2_dep = cc.find_library('func2')
test('test1', executable('test1', 'test1.c', dependencies : func2_dep))

# Verify that installed libfunc4.a is usable
func4_dep = cc.find_library('func4')
test('test2', executable('test2', 'test2.c', dependencies : func4_dep))

# Verify that installed pkg-config file is usable for both shared and static link
func6_static_dep = dependency('func6', static : true)
test('test3-static', executable('test3-static', 'test3.c',
    dependencies : func6_static_dep))
func6_shared_dep = dependency('func6', static : false)
test('test3-shared', executable('test3-shared', 'test3.c',
    dependencies : func6_shared_dep))

# Verify that installed libfunc9.a contains func8() and func8() but not func7()
func7_dep = cc.find_library('func7')
func9_linkwhole_dep = cc.find_library('func9_linkwhole')
test('test4-linkwhole', executable('test4-linkwhole', 'test4.c',
  dependencies : [func7_dep, func9_linkwhole_dep]))
func9_linkwith_dep = cc.find_library('func9_linkwith')
test('test4-linkwith', executable('test4-linkwith', 'test4.c',
  dependencies : [func7_dep, func9_linkwith_dep]))

# Verify that installed libfunc16.a is usable
libfunc16_dep = cc.find_library('func16')
test('test5', executable('test5', 'test5.c', dependencies: libfunc16_dep))
