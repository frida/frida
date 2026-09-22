project('master', ['cpp'])

foo = subproject('foo')
dep = foo.get_variable('foo_dep')

executable('master', 'master.cpp', dependencies: dep)
