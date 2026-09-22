project('pkg-config static', 'c')

if build_machine.system() != 'windows'
  prefix = meson.source_root()
else
  # pkg-config files should not use paths with \
  prefix_parts = meson.source_root().split('\\')
  # If the path is C:/foo/bar, convert it to /c/foo/bar so we can test if our
  # automatic conversion to C:/foo/bar inside PkgConfigDependency is working.
  if prefix_parts[0][1] == ':'
    drive = prefix_parts[0][0]
  else
    drive = prefix_parts[0]
  endif
  new_parts = []
  foreach part : prefix_parts
    if part != prefix_parts[0]
      new_parts += part
    endif
  endforeach
  prefix = '/@0@/@1@'.format(drive, '/'.join(new_parts))
endif
message(prefix)

# Escape spaces
prefix_parts = prefix.split(' ')
prefix = '\ '.join(prefix_parts)

conf = configuration_data()
conf.set('PREFIX', prefix)
configure_file(input : 'foo.pc.in',
               output : 'foo.pc',
               configuration : conf)

foo_dep = dependency('foo', static : true)

test('footest', executable('foomain', 'main.c', dependencies : foo_dep))
