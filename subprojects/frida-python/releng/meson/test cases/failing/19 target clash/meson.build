project('clash', 'c')

# This setup causes a namespace clash when two Meson targets would
# produce a Ninja targets with the same name. It only works on
# unix, because on Windows the target has a '.exe' suffix.
#
# This test might fail to work on different backends or when
# output location is redirected.

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST test only works on platforms where executables have no suffix.')
endif

executable('clash', 'clash.c')
run_target('clash', command: ['echo', 'clash 1'])
