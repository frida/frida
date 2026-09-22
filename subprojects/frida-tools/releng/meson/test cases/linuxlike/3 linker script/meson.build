project('linker script', 'c')

# Solaris 11.4 ld supports --version-script only when you also specify
# -z gnu-version-script-compat
if meson.get_compiler('c').get_linker_id() == 'ld.solaris'
  add_project_link_arguments('-Wl,-z,gnu-version-script-compat', language: 'C')
endif

# Static map file
mapfile = 'bob.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

l = shared_library('bob', 'bob.c', link_args : vflag, link_depends : mapfile)
e = executable('prog', 'prog.c', link_with : l)
test('core', e)

# configure_file
conf = configuration_data()
conf.set('in', 'bobMcBob')
m = configure_file(
  input : 'bob.map.in',
  output : 'bob-conf.map',
  configuration : conf,
)
vflag = '-Wl,--version-script,@0@'.format(meson.current_build_dir() / 'bob-conf.map')

l = shared_library('bob-conf', 'bob.c', link_args : vflag, link_depends : m)
e = executable('prog-conf', 'prog.c', link_with : l)
test('core', e)

# custom_target
python = find_program('python3')
m = custom_target(
  'bob-ct.map',
  command : [python, '@INPUT0@', '@INPUT1@', 'bob-ct.map'],
  input : ['copy.py', 'bob.map'],
  output : 'bob-ct.map',
  depend_files : 'bob.map',
)
vflag = '-Wl,--version-script,@0@'.format(m.full_path())

l = shared_library('bob-ct', ['bob.c', m], link_args : vflag, link_depends : m)
e = executable('prog-ct', 'prog.c', link_with : l)
test('core', e)

subdir('sub')

# With map file in subdir
mapfile = 'sub/foo.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

l = shared_library('bar', 'bob.c', link_args : vflag, link_depends : mapfile)
e = executable('prog-bar', 'prog.c', link_with : l)
test('core', e)
