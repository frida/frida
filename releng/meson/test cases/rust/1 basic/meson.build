project('rustprog', 'rust', default_options : ['b_ndebug=true'])

e = executable('rust-program', 'prog.rs',
  rust_args : ['-C', 'lto'], # Just a test
  install : true
)
test('rusttest', e)

subdir('subdir')

# this should fail due to debug_assert
test(
  'debug_assert_on',
  executable(
    'rust-program2',
    'prog.rs',
    override_options : ['b_ndebug=false'],
  ),
  should_fail : true,
)

rustc = meson.get_compiler('rust')
assert(rustc.run('fn main(){}').returncode() == 0)
