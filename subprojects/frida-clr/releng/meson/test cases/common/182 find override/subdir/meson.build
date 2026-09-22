x = find_program('converter.py')

meson.override_find_program('codegen', x)

# Override a command with a generated script

cdata = configuration_data()

cdata.set('NUMBER', 100)
numprog = configure_file(input : 'gencodegen.py.in',
  output : 'gencodegen.py',
  configuration : cdata)

meson.override_find_program('gencodegen', numprog)
