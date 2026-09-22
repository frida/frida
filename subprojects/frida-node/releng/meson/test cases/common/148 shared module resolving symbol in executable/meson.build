project('shared module resolving symbol in executable', 'c')

# The shared module contains a reference to the symbol 'func_from_executable',
# which is always provided by the executable which loads it.  This symbol can be
# resolved at run-time by an ELF loader.  But when building PE/COFF objects, all
# symbols must be resolved at link-time, so an implib is generated for the
# executable, and the shared module linked with it.
#
# See testcase 125 for an example of the more complex portability gymnastics
# required if we do not know (at link-time) what provides the symbol.

cc = meson.get_compiler('c')
if cc.get_id() == 'pgi'
  error('MESON_SKIP_TEST PGI has its own unique set of macros that would need to be handled')
endif

dl = meson.get_compiler('c').find_library('dl', required: false)
e = executable('prog', 'prog.c', dependencies: dl, export_dynamic: true)
e_dep = declare_dependency(link_with: e)

m = shared_module('module', 'module.c', link_with: e)
m2 = shared_module('module2', 'module.c', dependencies: e_dep)

test('test', e, args: m.full_path())
test('test2', e, args: m2.full_path())
