project('statchain', 'c')

subdir('subdir')
# Test that -fPIC in c_args is also accepted (on platforms where it's permitted)
picflag = []
if not ['darwin', 'windows'].contains(host_machine.system())
  picflag = ['-fPIC']
endif
statlib2 = static_library('stat2', 'stat2.c', c_args : picflag, pic : false)
# Test that pic is needed for both direct and indirect static library
# dependencies of shared libraries (on Linux and BSD)
statlib = static_library('stat', 'stat.c', link_with : [shlib, statlib2], pic : true)
shlib2 = shared_library('shr2', 'shlib2.c', link_with : statlib)
exe = executable('prog', 'prog.c', link_with : shlib2)
test('runtest', exe)
