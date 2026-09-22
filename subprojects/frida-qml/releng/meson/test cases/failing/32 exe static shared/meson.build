project('statchain', 'c')

host_system = host_machine.system()
if host_system == 'windows' or host_system == 'darwin'
  error('MESON_SKIP_TEST test only fails on Linux and BSD')
endif

statlib = static_library('stat', 'stat.c', pic : false)
shlib2 = shared_library('shr2', 'shlib2.c', link_with : statlib)
exe = executable('prog', 'prog.c', link_with : shlib2)
test('runtest', exe)
