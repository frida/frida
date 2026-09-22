project('postconf script', 'c')

conf = configure_file(
  configuration : configuration_data(),
  output : 'out'
)

meson.add_postconf_script(find_program('postconf.py'), '5', '33', conf)

test('post', executable('prog', 'prog.c'))
