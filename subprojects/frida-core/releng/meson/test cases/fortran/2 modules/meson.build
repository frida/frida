project('modules', 'fortran',
  default_options : ['default_library=static'])

commented = library('commented', 'comment_mod.f90')

# Have one file with an upper case file extension.
e = executable('modprog', 'mymod.F90', 'prog.f90',
  link_with: commented)
test('moduletest', e)
