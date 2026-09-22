project('cpp std override', 'cpp',
  default_options : ['cpp_std=c++98',
                     'werror=true'])

executable('plain', 'progp.cpp',
  override_options : 'cpp_std=none')
executable('v98', 'prog98.cpp',
  override_options : 'werror=false')
executable('v11', 'prog11.cpp',
  override_options : 'cpp_std=c++11')
