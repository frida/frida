project('keyval builddir test')

k = import('keyval')

out_conf = configure_file(input: 'config', output: 'out-config', copy: true)
conf = k.load(out_conf)

if not conf.has_key('CONFIG_IS_SET')
  error('Expected CONFIG_IS_SET to be set, but it wasn\'t')
endif

if conf.has_key('CONFIG_NOT_IS_SET')
  error('Expected CONFIG_NOT_IS_SET not be set, but it was')
endif
