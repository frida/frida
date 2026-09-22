project('xcode extra framework test', 'c')

opengl_dep = dependency('OpenGL', method : 'extraframework')
assert(opengl_dep.type_name() == 'extraframeworks', 'type_name is ' + opengl_dep.type_name())

dep_main = dependency('Foundation')
assert(dep_main.type_name() == 'extraframeworks', 'type_name is ' + dep_main.type_name())

# https://github.com/mesonbuild/meson/issues/10002
ldap_dep = dependency('LDAP', method : 'extraframework')
assert(ldap_dep.type_name() == 'extraframeworks', 'type_name is ' + ldap_dep.type_name())

stlib = static_library('stat', 'stat.c', install : true, dependencies: [opengl_dep, ldap_dep])
exe = executable('prog', 'prog.c', install : true, dependencies: dep_main)
