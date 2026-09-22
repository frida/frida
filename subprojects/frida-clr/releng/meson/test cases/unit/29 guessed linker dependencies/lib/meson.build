project('lib1', ['c'])

c_args = []

# Microsoft's compiler is quite smart about touching import libs on changes,
# so ensure that there is actually a change in symbols.
if get_option('more_exports')
    c_args += '-DMORE_EXPORTS'
endif

a = library('test-lib', 'lib.c', c_args: c_args, install: true)
