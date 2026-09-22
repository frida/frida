project('generated rust main', 'rust')

gen = find_program('gen.py')

c = custom_target(
  'custom_target',
  command : [gen, '@OUTPUT@'],
  output : ['main.rs'],
)

executable('custom_target_main', c)
executable('custom_target_index_main', c[0])

gener = generator(gen, arguments : ['@OUTPUT@'], output : '@BASENAME@.rs')
# Doesn't actually use gen.py as input, just a limitation of generators
executable('generator_main', gener.process(['gen.py']))

subdir('sub')
executable('custom_target_subdir_main', s)

executable('link_with_generated_lib', 'generated_lib_main.rs', link_with : lib)
