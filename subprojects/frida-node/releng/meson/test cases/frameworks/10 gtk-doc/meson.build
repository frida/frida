project('gtkdoctest', 'c', version : '1.0.0')

gtkdoc = find_program('gtkdoc-scan', required: false)
if not gtkdoc.found()
  error('MESON_SKIP_TEST gtkdoc not found.')
endif

gnome = import('gnome')

assert(gnome.gtkdoc_html_dir('foobar') == 'share/gtk-doc/html/foobar', 'Gtkdoc install dir is incorrect.')

inc = include_directories('include')

subdir('include')

# disable this test unless a bug fix for spaces in pathnames is present
# https://bugzilla.gnome.org/show_bug.cgi?id=753145
result = run_command(gtkdoc, ['--version'], check: true)
gtkdoc_ver = result.stdout().strip()
if gtkdoc_ver == ''
  gtkdoc_ver = result.stderr().strip()
endif
if gtkdoc_ver.version_compare('<1.26')
  error('MESON_SKIP_TEST gtk-doc test requires gtkdoc >= 1.26.')
endif

gobject = dependency('gobject-2.0')

libfoo = shared_library('foo', 'foo.c',
  include_directories: inc,
  dependencies: gobject,
)

deps = []
if host_machine.system() == 'darwin'
  deps += dependency('appleframeworks', modules : ['Foundation', 'CoreFoundation'])
endif

foo_dep = declare_dependency(
  link_with: libfoo,
  include_directories: inc,
  dependencies: deps,
)

subdir('doc')
