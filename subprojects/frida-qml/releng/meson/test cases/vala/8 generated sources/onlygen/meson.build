onlygen = generator(copy,
  output : '@BASENAME@.vala',
  arguments : ['@INPUT@', '@OUTPUT@'])

executable('onlygentest', onlygen.process('maingen.in'),
  install : true,
  dependencies: [dependency('glib-2.0'), dependency('gobject-2.0')])
