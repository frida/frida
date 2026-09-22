cdata = configuration_data()
cdata.set('VERSION', '1.0')
version_xml = configure_file(input : 'version.xml.in',
                             output : 'version.xml',
                             configuration : cdata)

subdir('foobar1')
subdir('foobar2')
subdir('foobar3')
subdir('foobar4')
