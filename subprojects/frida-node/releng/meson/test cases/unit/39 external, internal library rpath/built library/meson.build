project('built library', 'c')

cc = meson.get_compiler('c')

import('python').find_installation().install_sources('foo.py')

if host_machine.system() != 'cygwin'
  # bar_in_system has undefined symbols, but still must be found
  bar_system_dep = cc.find_library('bar_in_system')
endif

foo_system_dep = cc.find_library('foo_in_system')

faa_pkg_dep = dependency('faa_pkg')

l = shared_library('bar_built', 'bar.c',
                   install: true,
                   dependencies : [foo_system_dep, faa_pkg_dep])

if host_machine.system() == 'darwin'
  e = executable('prog', 'prog.c', link_with: l, install: true)
  test('testprog', e)
elif host_machine.system() == 'linux'
  e = executable('prog', 'prog.c', link_with: l, install: true,
        install_rpath: '$ORIGIN/..' / get_option('libdir'),
      )
  test('testprog', e)
endif
