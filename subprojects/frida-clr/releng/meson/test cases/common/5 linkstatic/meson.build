project('static library linking test', 'c')

lib = build_target('mylib', 'libfile.c', 'libfile2.c', 'libfile3.c', 'libfile4.c', target_type : 'static_library')
exe = executable('prog', 'main.c', link_with : lib)

test('runtest', exe)
