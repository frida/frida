
k = import('keyval')

conf = k.load(files('config'))

if not conf.has_key('CONFIG_IS_SET')
  error('Expected CONFIG_IS_SET to be set, but it wasn\'t')
endif

if conf.has_key('CONFIG_NOT_IS_SET')
  error('Expected CONFIG_NOT_IS_SET not be set, but it was')
endif
