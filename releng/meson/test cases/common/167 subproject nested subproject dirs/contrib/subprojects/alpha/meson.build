project('alpha project', 'c', subproject_dir: 'var/subprojects')

b = subproject('beta')
l = library('a', 'a.c', link_with : b.get_variable('lb'))
