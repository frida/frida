project('exe', ['c'])

executable('app',
  'app.c',
  # Use uninterpreted strings to avoid path finding by dependency or compiler.find_library
  link_args: ['-ltest-lib']
  )
