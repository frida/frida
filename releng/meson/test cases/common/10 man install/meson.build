project('man install')
m1 = install_man('foo.1')
m2 = install_man('bar.2')
m3 = install_man('foo.fr.1', locale: 'fr')
install_man('vanishing/vanishing.2')
subdir('vanishing')

cdata = configuration_data()
cdata.set('TODAY', '$this_day')
b1 = configure_file(input : 'baz.1.in',
  output : 'baz.1',
  configuration : cdata)

install_man(b1)
