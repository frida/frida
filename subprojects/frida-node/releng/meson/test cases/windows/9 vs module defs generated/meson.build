project('generated_dll_module_defs', 'c')

subdir('subdir')
exe = executable('prog', 'prog.c', link_with : shlib, vs_module_defs : 'exe.def')
test('runtest', exe)
