project('multiple outputs install')

gen = find_program('generator.py')

custom_target('different-install-dirs',
  output : ['diff.h', 'diff.sh'],
  command : [gen, 'diff', '@OUTDIR@'],
  install : true,
  install_dir : [join_paths(get_option('prefix'), get_option('includedir')),
                 join_paths(get_option('prefix'), get_option('bindir'))])

custom_target('same-install-dir',
  output : ['same.h', 'same.sh'],
  command : [gen, 'same', '@OUTDIR@'],
  install : true,
  install_dir : '/opt')

custom_target('only-install-first',
  output : ['first.h', 'first.sh'],
  command : [gen, 'first', '@OUTDIR@'],
  install : true,
  install_dir : [join_paths(get_option('prefix'), get_option('includedir')), false])

targets = custom_target('only-install-second',
  output : ['second.h', 'second.sh'],
  command : [gen, 'second', '@OUTDIR@'],
  install : true,
  install_dir : [false, join_paths(get_option('prefix'), get_option('bindir'))])

paths = []
foreach i : targets.to_list()
  paths += i.full_path()
endforeach

# The Xcode backend has a different output naming scheme.
if meson.backend() == 'xcode'
  assert(paths == [meson.project_build_root() / get_option('buildtype') / 'second.h',
                   meson.project_build_root() / get_option('buildtype') / 'second.sh'])

# Skip on Windows because paths are not identical, '/' VS '\'.
elif host_machine.system() != 'windows'
  assert(paths == [meson.current_build_dir() / 'second.h',
                   meson.current_build_dir() / 'second.sh'])
endif
