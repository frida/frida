project('set10test', 'c')

conf = configuration_data()
conf.set10('ONE', true)
conf.set10('ZERO', false)

configure_file(input : 'config.h.in',
 output : 'config.h',
  configuration : conf)

exe = executable('prog', 'prog.c')
test('10test', exe)
