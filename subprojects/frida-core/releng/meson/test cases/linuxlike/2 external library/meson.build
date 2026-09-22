project('external library', 'c')

cc = meson.get_compiler('c')
zlib = cc.find_library('z')

# Verify that link testing works.
linkcode = '''#include<zlib.h>
int main(void) {
  void *ptr = (void*)(deflate);
  return ptr == 0;
}
'''

nolinkcode='''int nonexisting();
int main(void) {
  return nonexisting();
}
'''

assert(cc.links(linkcode, args : '-lz', name : 'Test link against zlib'), 'Linking test failed.')
d1 = declare_dependency(compile_args: '-DSOMETHING', link_args: '-lz')
assert(cc.links(linkcode, dependencies : d1,
                name : 'Test link against zlib via declare_dependency'), 'Linking test failed.')
d2 = declare_dependency(dependencies: d1)
assert(cc.links(linkcode, dependencies : d2,
                name : 'Test link against zlib via indirect declare_dependency'), 'Linking test failed.')
assert(not cc.links(nolinkcode, name : 'Failing link'), 'Linking succeeded when it should have failed.')

e = executable('zprog', 'prog.c', dependencies : zlib)
test('libtest', e)

e2 = executable('zprog_alt', 'prog.c', dependencies : zlib)
test('libtest_alt', e2)

# Test that ext deps work via an internal dep.
intdep = declare_dependency(dependencies : zlib)
exe2 = executable('zprog2', 'prog.c', dependencies : intdep)
test('libtest2', exe2)

# Test that deps that use find_library deps work.
depdep = declare_dependency(dependencies : intdep)
exe3 = executable('zprog3', 'prog.c', dependencies : depdep)
test('libtest3', exe3)
