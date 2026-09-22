project('has function ext dep', 'c')

cc = meson.get_compiler('c')

mylib = shared_library('mylib', 'mylib.c')
mylib_dep = declare_dependency(link_with : mylib)
# Only external dependencies can work here
cc.has_function('malloc', dependencies : mylib_dep)
