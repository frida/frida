project('both libraries linking test', 'c', 'cpp')

both_libs = both_libraries('mylib', 'libfile.c')
dep = declare_dependency(link_with: both_libs)
exe_shared = executable('prog-shared', 'main.c', link_with : both_libs.get_shared_lib())
exe_static = executable('prog-static', 'main.c',
                        c_args : ['-DSTATIC_COMPILATION'],
                        link_with : both_libs.get_static_lib())
exe_both = executable('prog-both', 'main.c', link_with : both_libs)
exe_dep = executable('prog-dep', 'main.c', dependencies : [dep])

# Try using it in a custom_target
custom_target('tgt_a',
    command: [
        find_program('./dummy.py'),
        '@OUTPUT@',
        both_libs,
    ],
    output: ['hello1.txt'],
    input: [both_libs],
)

test('runtest-shared', exe_shared)
test('runtest-static', exe_static)
test('runtest-both', exe_both)
test('runtest-dep', exe_dep)

# Same as above, but using build_target()
both_libs2 = build_target('mylib2', 'libfile.c', target_type: 'both_libraries')
exe_shared2 = executable('prog-shared2', 'main.c',
                         link_with : both_libs2.get_shared_lib())
exe_static2 = executable('prog-static2', 'main.c',
                         c_args : ['-DSTATIC_COMPILATION'],
                         link_with : both_libs2.get_static_lib())
exe_both2 = executable('prog-both2', 'main.c', link_with : both_libs2)

# Test {set,get}_variable
set_variable('both_libs2', both_libs)
both_libs3 = get_variable('both_libs')

# Ensure that calling the build target methods also works
assert(both_libs.name() == 'mylib')
assert(both_libs2.name() == 'mylib')
assert(both_libs3.name() == 'mylib')
assert(both_libs2.get_shared_lib().name() == 'mylib')
assert(both_libs3.get_static_lib().name() == 'mylib')

test('runtest-shared-2', exe_shared2)
test('runtest-static-2', exe_static2)
test('runtest-both-2', exe_both2)

# Regression test: libccpp has both C and C++ sources. The executable only has
# C sources. It should still link using the C++ compiler. When using
# both_libraries the static has no sources and thus no compilers, resulting in
# the executable linking using the C compiler.
# https://github.com/Netflix/vmaf/issues/1107
libccpp = both_libraries('ccpp', 'foo.cpp', 'libfile.c',
  cpp_args : ['-std=c++11'],
  c_static_args : ['-DSTATIC_COMPILATION'],
  cpp_static_args : ['-DSTATIC_COMPILATION'],
)
exe = executable('prog-ccpp', 'main2.c',
  link_with: libccpp.get_static_lib(),
  c_args : ['-DSTATIC_COMPILATION'],
)
test('runtest-ccpp', exe)

exe = executable('prog-ccpp-shared', 'main2.c',
  link_with: libccpp.get_shared_lib(),
)
test('runtest-ccpp-shared', exe)
