project('hidden symbol', 'c')

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST -fvisibility=hidden does not work for PE files.')
endif

l = shared_library('bob', 'bob.c',
  gnu_symbol_visibility: 'hidden')

executable('bobuser', 'bobuser.c',
  link_with: l)
