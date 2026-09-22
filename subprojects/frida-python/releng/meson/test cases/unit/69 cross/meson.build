project('crosstest')

if get_option('generate')
    conf_data = configuration_data()
    conf_data.set('system', build_machine.system())
    conf_data.set('cpu', build_machine.cpu())
    conf_data.set('cpu_family', build_machine.cpu_family())
    conf_data.set('endian', build_machine.endian())

    configure_file(input: 'crossfile.in',
                output: 'crossfile',
                configuration: conf_data)
    message('Written cross file')
else
    assert(meson.is_cross_build(), 'not setup as cross build')
endif
