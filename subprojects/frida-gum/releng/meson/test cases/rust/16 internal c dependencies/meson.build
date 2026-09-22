project('internal dependencies', 'c', 'rust')

test_prog = find_program('test.py')

static = static_library('static', 'lib.c', c_args : '-DMODE="static"')
# Add some -I compiler arguments to make sure they're not passed to the Rust
# compiler when handling the dependency.
static_dep = declare_dependency(link_with : static, compile_args : ['-Idoesnotexist'])
exe = executable('static', 'main.rs', dependencies : static_dep)
test('static linkage', test_prog, args : [exe, 'This is a static C library'])

# Shared linkage with rust doesn't work on macOS with meson, yet
if host_machine.system() != 'darwin'
  shared = shared_library('shared', 'lib.c', c_args : '-DMODE="shared"')
  shared_dep = declare_dependency(link_with : shared, compile_args : ['-Idoesnotexist'])
  exe = executable('shared', 'main.rs', dependencies : shared_dep)
  test('shared linkage', test_prog, args : [exe, 'This is a shared C library'])
endif
