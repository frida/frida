project('test static link libs', 'c')

pkg = import('pkgconfig')

# libfunc2 should contain both func1() and func2() symbols
libfunc1 = static_library('func1', 'func1.c',
  install : false)
libfunc2 = static_library('func2', 'func2.c',
  link_whole : libfunc1,
  install : true)

# Same as above, but with link_with instead of link_whole,
# libfunc4 should contain both func3() and func4() symbols
libfunc3 = static_library('func3', 'func3.c',
  install : false)
libfunc4 = static_library('func4', 'func4.c',
  link_with : libfunc3,
  install : true)

# Same as above, but also generate an pkg-config file. Use both_libraries() to
# make sure a complete .pc file gets generated. libfunc5 should not be mentioned
# into the .pc file because it's not installed.
libfunc5 = static_library('func5', 'func5.c',
  install : false)
libfunc6 = both_libraries('func6', 'func6.c',
  link_with : libfunc5,
  install : true)
pkg.generate(libfunc6)

# libfunc9 should contain both func8() and func9() but not func7() because that
# one gets installed. Also test that link_with and link_whole works the same way
# because libfunc8 is uninstalled.
libfunc7 = static_library('func7', 'func7.c',
  install : true)
libfunc8 = static_library('func8', 'func8.c',
  link_with : libfunc7,
  install : false)
libfunc9_linkwith = static_library('func9_linkwith', 'func9.c',
  link_with : libfunc8,
  install : true)
libfunc9_linkwhole = static_library('func9_linkwhole', 'func9.c',
  link_whole : libfunc8,
  install : true)

# Pattern found in mesa:
# - libfunc11 uses func10()
# - libfunc12 uses both func10() and func11()
# When a shared library link_whole on libfunc12, we ensure we don't include
# func10.c.o twice which would fail to link.
libfunc10 = static_library('func10', 'func10.c',
  install : false)
libfunc11 = static_library('func11', 'func11.c',
  link_with : libfunc10,
  install : false)
libfunc12 = static_library('func12', 'func12.c',
  link_with : [libfunc10, libfunc11],
  install : false)
libfunc13 = shared_library('func13', link_whole : libfunc12)

# libfunc16 should contain func14(), func15() and func16()
libfunc14 = static_library('func14', 'func14.c',
  install : false)
libfunc15 = static_library('func15', 'func15.c',
  link_with : libfunc14,
  install : false)
libfunc16 = static_library('func16', 'func16.c',
  link_with : libfunc15,
  install : true)

# Verify func17.c.o gets included only once into libfunc19, otherwise
# func19-shared would failed with duplicated symbol.
libfunc17 = static_library('func17', 'func17.c',
  install : false)
libfunc18 = static_library('func18', 'func18.c',
  link_with : libfunc17,
  install : false)
libfunc19 = static_library('func19', 'func19.c',
  link_whole : [libfunc17, libfunc18],
  install : false)
shared_library('func19-shared', link_whole : [libfunc19])
