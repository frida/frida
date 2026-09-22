project('linkcustom', 'c')

# This would require passing the static linker to the build script or having
# it detect it by itself. I'm too lazy to implement it now and it is not
# really needed for testing that custom targets work. It is the responsibility
# of the custom target to produce things in the correct format.
assert(not meson.is_cross_build(),
       'MESON_SKIP_TEST cross checking not implemented.')

cc = meson.get_compiler('c')
genprog = find_program('generate_stlibs.py')

clib = custom_target('linkcustom',
  output: ['libflob_1.a', 'libflob_2.a'],
  command: [genprog,
            '-o', '@OUTPUT@',
            '--private-dir', '@PRIVATE_DIR@'] + cc.cmd_array())

clibs = [clib[0], clib[1]]

exe = executable('prog', 'prog.c', link_with: clibs)
test('linkcustom', exe)

d = declare_dependency(link_with: clibs)

exe2 = executable('prog2', 'prog.c', dependencies: d)
test('linkcustom2', exe2)

# Link whole tests

if meson.backend() == 'xcode'
    message('Xcode does not support link whole so skipping.')
    subdir_done()
endif

exe3 = executable('prog3', 'prog.c', link_whole: clibs)
test('linkwhole', exe)

d2 = declare_dependency(link_whole: clibs)

exe4 = executable('prog4', 'prog.c', dependencies: d2)
test('linkwhole2', exe2)
