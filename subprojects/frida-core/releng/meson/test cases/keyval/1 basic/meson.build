project('keyval basic test', meson_version : '>= 0.55')

k = import('keyval')
conf = k.load('.config')

if not conf.has_key('CONFIG_VAL1')
  error('Expected CONFIG_VAL1 to be set, but it wasn\'t')
endif

if conf.has_key('CONFIG_VAL2')
  error('Expected CONFIG_VAL2 not be set, but it was')
endif

if conf.get('CONFIG_VAL_VAL').to_int() != 4
  error('Expected CONFIG_VAL_VAL to be 4')
endif

k = import('unstable-keyval')
