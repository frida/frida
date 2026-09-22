project('subproj user', 'c',
  version : '2.3.4',
  license : 'mylicense',
  license_files: 'mylicense.txt',
)

assert(meson.project_name() == 'subproj user', 'Incorrect project name')

sub = subproject('sublib', version : '1.0.0')

if meson.project_version() != '2.3.4'
  error('Incorrect master project version string:' + meson.project_version())
endif

if meson.is_subproject()
  error('Claimed to be a subproject even though we are the master project.')
endif

inc = sub.get_variable('i')
lib = sub.get_variable('l')

e = executable('user', 'user.c', include_directories : inc, link_with : lib, install : true)
test('subdirtest', e)

meson.install_dependency_manifest('share/sublib/sublib.depmf')

unknown_var = sub.get_variable('does-not-exist', [])
if unknown_var != []
  error ('unexpected fallback value for subproject.get_variable()')
endif
