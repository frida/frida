project('gobject-introspection', 'c')

copyfile = find_program('copyfile.py')
copyfile_gen = generator(copyfile,
  output: '@BASENAME@Gen.xml',
  arguments : ['@INPUT@', '@OUTPUT@'])

glib = dependency('glib-2.0', required: false)
if not glib.found()
  error('MESON_SKIP_TEST glib not found.')
endif

gir = dependency('gobject-introspection-1.0', required: false)
if not gir.found()
  error('MESON_SKIP_TEST gobject-introspection not found.')
endif

python3 = import('python3')
py3 = python3.find_python()
if run_command(py3, '-c', 'import gi;', check: false).returncode() != 0
    error('MESON_SKIP_TEST python3-gi not found')
endif

cc = meson.get_compiler('c')

add_global_arguments('-DMESON_TEST_1', language : 'c')
if cc.get_id() == 'intel'
  # Ignore invalid GCC pragma warnings from glib
  # https://bugzilla.gnome.org/show_bug.cgi?id=776562
  add_global_arguments('-wd2282', language : 'c')
endif

py3 = import('python3').find_python()
pycode = '''import os, sys
if "MESON_UNIT_TEST_PRETEND_GLIB_OLD" in os.environ:
  sys.exit(0)
sys.exit(1)
'''

pretend_glib_old = false
res = run_command(py3, '-c', pycode, check: false)
if res.returncode() == 0
  pretend_glib_old = true
endif

gnome = import('gnome')
gio = dependency('gio-2.0')
giounix = dependency('gio-unix-2.0')
glib = dependency('glib-2.0')
gobj = dependency('gobject-2.0')
gir = dependency('gobject-introspection-1.0')
gmod = dependency('gmodule-2.0')

# GLib >= 2.76 removed slice allocator which causes a leak in g-i to now be
# visible to asan. The leak should be fixed in g-i >= 1.76.2:
# https://gitlab.gnome.org/GNOME/gobject-introspection/-/merge_requests/411
if get_option('b_sanitize') != 'none' and \
   gir.version().version_compare('<=1.76.1') and \
   glib.version().version_compare('>=2.76')
  error('MESON_SKIP_TEST gobject-introspection >=1.76.2 is required with address sanitizer.')
endif

# Test that static deps don't error out when static libraries aren't found
glib_static = dependency('glib-2.0', static : true)

subdir('resources-data')
subdir('resources')
subdir('gir')
subdir('schemas')
subdir('gdbus')
subdir('mkenums')
subdir('genmarshal')
