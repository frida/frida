project('static rust and c polyglot executable', 'c', 'rust')

r = static_library('stuff', 'stuff.rs', rust_crate_type : 'staticlib')

# clib is installed static library and stuff is not installed. That means that
# to be usable clib must link_whole stuff. Meson automatically promote to link_whole,
# as it would do with C libraries, but then cannot extract objects from stuff and
# thus should error out.
# FIXME: We should support this use-case in the future.
testcase expect_error('Cannot link_whole a custom or Rust target \'stuff\' into a static library \'clib\'. Instead, pass individual object files with the "objects:" keyword argument if possible. Meson had to promote link to link_whole because \'clib\' is installed but not \'stuff\', and thus has to include objects from \'stuff\' to be usable.')
  l = static_library('clib', 'clib.c', link_with : r, install : true)
endtestcase

l = static_library('clib', 'clib.c', link_with : r)

e = executable('prog', 'prog.c',
               link_with : l,
               install : true)
test('polyglottest', e)

# Create a version that has overflow-checks on, then run a test to ensure that
# the overflow-checks is larger than the other version by some ammount
r2 = static_library('stuff2', 'stuff.rs', rust_crate_type : 'staticlib', rust_args : ['-C', 'overflow-checks=on'])
l2 = static_library('clib2', 'clib.c')
e2 = executable('prog2', 'prog.c', link_with : [r2, l2])
test('overflow-checks', find_program('overflow_size_checks.py'), args : [e, e2])
