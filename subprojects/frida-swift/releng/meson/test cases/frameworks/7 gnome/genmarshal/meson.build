m_list = configure_file(input: 'marshaller.list',
  output: 'm.list',
  copy: true)

idx = 0
mlists = ['marshaller.list', files('marshaller.list'), m_list]

foreach mlist : mlists
  marshallers = gnome.genmarshal('marshaller-@0@'.format(idx),
  sources : mlist,
  install_header : true,
  install_dir : get_option('includedir') / 'subdir-@0@'.format(idx),
  extra_args : ['-UG_ENABLE_DEBUG', '--prototypes'])

  marshaller_c = marshallers[0]
  marshaller_h = marshallers[1]

  cdata = configuration_data()
  cdata.set_quoted('MARSHALLER_HEADER', 'marshaller-@0@.h'.format(idx))

  main_c = configure_file(input: 'main.c.in',
    output: 'main-@0@.c'.format(idx),
    configuration: cdata)

  genmarshalexe = executable('genmarshalprog-@0@'.format(idx),
    main_c, marshaller_c, marshaller_h,
    dependencies : gobj)
  test('genmarshal test @0@'.format(idx), genmarshalexe)
  idx += 1
endforeach

foreach mlist : mlists
  marshallers = gnome.genmarshal('marshaller-@0@'.format(idx),
  sources : [mlist],
  install_header : true,
  install_dir : get_option('includedir') / 'subdir-@0@'.format(idx),
  extra_args : ['-UG_ENABLE_DEBUG', '--prototypes'])

  marshaller_c = marshallers[0]
  marshaller_h = marshallers[1]

  cdata = configuration_data()
  cdata.set_quoted('MARSHALLER_HEADER', 'marshaller-@0@.h'.format(idx))

  main_c = configure_file(input: 'main.c.in',
    output: 'main-@0@.c'.format(idx),
    configuration: cdata)

  genmarshalexe = executable('genmarshalprog-@0@'.format(idx),
    main_c, marshaller_c, marshaller_h,
    dependencies : gobj)
  test('genmarshal test @0@'.format(idx), genmarshalexe)
  idx += 1
endforeach
