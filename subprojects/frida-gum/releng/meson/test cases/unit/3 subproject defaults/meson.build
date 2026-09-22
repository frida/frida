project('subproject defaults', 'c',
  default_options : ['defopoverride=defopt', # This should be overridden.
                     'fromcmdline=defopt']   # This should get the value set in command line.
  )

subproject('foob', default_options : ['fromspfunc=spfunc', 'fromspfunconly=spfunc'])

assert(get_option('fromcmdline') == 'cmdline', 'Default option defined in cmd line is incorrect: ' + get_option('fromcmdline'))
assert(get_option('defopoverride') == 'defopt', 'Default option without cmd line override is incorrect: ' + get_option('defopoverride'))
assert(get_option('fromoptfile') == 'optfile', 'Default value from option file is incorrect: ' + get_option('fromoptfile'))
