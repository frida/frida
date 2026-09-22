conf = configuration_data()
conf.set('func', 'somedllfunc')
def_file = configure_file(
  input: 'somedll.def.in',
  output: 'somedll.def',
  configuration : conf,
)

shlib = shared_library('somedll', 'somedll.c', vs_module_defs : def_file)
shmod = shared_module('somemod', 'somedll.c', vs_module_defs : def_file)
