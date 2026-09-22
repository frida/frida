project('rust proc-macro', 'rust')

if build_machine.system() != 'linux'
    error('MESON_SKIP_TEST, this test only works on Linux. Patches welcome.')
endif

pm = shared_library(
  'proc_macro_examples',
  'proc.rs',
  rust_crate_type : 'proc-macro',
)

main = executable(
  'main',
  'use.rs',
  link_with : pm,
  rust_args : ['-C', 'panic=abort'],
)

test('main_test', main)

rust = import('rust')

pm = rust.proc_macro('proc_macro_examples2', 'proc.rs')

main = executable(
  'main2',
  'use.rs',
  link_with : pm,
  rust_dependency_map : {'proc_macro_examples2' : 'proc_macro_examples'}
)

test('main_test2', main)

subdir('subdir')

staticlib = static_library('staticlib', 'lib.rs',
  link_with: pm_in_subdir,
  rust_dependency_map : {'proc_macro_examples3' : 'proc_macro_examples'}
)

executable('transitive-proc-macro', 'transitive-proc-macro.rs',
  link_with: staticlib,
)
