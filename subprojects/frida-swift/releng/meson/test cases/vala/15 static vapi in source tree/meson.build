project('static vapi', 'c', 'vala')

glib = dependency('glib-2.0')

conf = configuration_data()
conf.set_quoted('VERSION', '1.0.0')
config_h = configure_file(output : 'config.h',
  configuration : conf)

e = executable('static-vapi', 'vapi/config.vapi', 'test.vala',
  dependencies : glib)

test('test-config', e)
