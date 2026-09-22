project('test', ['c', 'cpp'])

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# used by run_unittests.py to grab the path to the C and C++ compilers
assert(cc.compiles(files('test.c')))
assert(cxx.compiles(files('test.c')))

assert(cc.links(files('test.c')))
assert(cxx.links(files('test.c')))
