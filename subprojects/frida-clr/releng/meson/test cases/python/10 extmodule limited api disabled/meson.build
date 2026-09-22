project('Python limited api disabled', 'c',
  default_options : ['buildtype=release', 'werror=true', 'python.allow_limited_api=false'])

py_mod = import('python')
py = py_mod.find_installation()

module = py.extension_module('my_module',
  'module.c',
  limited_api: '3.7',
)
