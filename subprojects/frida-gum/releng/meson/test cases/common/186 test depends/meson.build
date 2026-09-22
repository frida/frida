project('test depends', 'c')

gen = find_program('gen.py')

custom_dep = custom_target('custom_dep',
  build_by_default : false,
  output : 'custom_dep.txt',
  command : [gen, '@OUTPUT@', 'custom_dep'],
)

exe_dep = executable('exe_dep', 'main.c',
  build_by_default : false,
)

test_prog = find_program('test.py')
test('string dependencies', test_prog,
  args : [
    # This is declared for convenience,
    # real use case might have some obscure method
    # to find these dependencies, e.g. automatic plugin loading.
    'custom_dep.txt',
    exe_dep.full_path(),
  ],
  depends : [custom_dep, exe_dep],
  workdir : meson.current_build_dir(),
)
