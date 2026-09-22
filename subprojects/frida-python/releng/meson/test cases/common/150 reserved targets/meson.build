project('reserved target names', 'c')
        # FIXME: Setting this causes it to leak to all other tests
        #default_options : ['b_coverage=true']

subdir('all')
subdir('benchmark')
subdir('clean')
subdir('clean-ctlist')
subdir('clean-gcda')
subdir('clean-gcno')
subdir('coverage')
subdir('coverage-html')
subdir('coverage-text')
subdir('coverage-xml')
subdir('dist')
subdir('distcheck')
subdir('install')
# We don't have a 'PHONY' directory because Windows and OSX
# choke horribly when there are two entries with the same
# name but different case.
subdir('phony')
subdir('reconfigure')
subdir('scan-build')
subdir('test')
subdir('uninstall')

subdir('runtarget')

py3 = import('python3').find_python()

custom_target('ctlist-test', output : 'out.txt',
              command : [py3, '-c', 'print("")'],
              capture : true,
              build_by_default : true)
