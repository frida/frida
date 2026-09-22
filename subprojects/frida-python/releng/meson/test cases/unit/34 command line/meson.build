project('command line test', 'c',
  default_options : ['default_library=static', 'set_sub_opt=true']
)

if get_option('set_sub_opt')
  subproject('subp', default_options : ['subp_opt=default3'])
else
  subproject('subp')
endif
