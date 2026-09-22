project('native file overrides')

foreach o : ['bindir', 'datadir', 'includedir', 'infodir', 'libdir',
             'libexecdir', 'localedir', 'localstatedir', 'mandir', 'prefix',
             'sbindir', 'sharedstatedir', 'sysconfdir']
  expected = get_option('def_' + o)
  actual = get_option(o)
  assert(expected == actual,
         '@0@ should have been @1@, but was @2@!'.format(o, expected, actual))
endforeach
