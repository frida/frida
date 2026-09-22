libsources = ['meson-subsample.c', 'meson-subsample.h']

girsubproject = shared_library(
  'girsubproject',
  sources : libsources,
  dependencies : [gobj, meson_gir],
  install : true
)

girexe = executable(
  'girprog',
  sources : 'prog.c',
  dependencies : [gobj, meson_gir],
  link_with : girsubproject
)

gnome.generate_gir(
  girsubproject,
  sources : libsources,
  dependencies : [gobj, meson_gir],
  nsversion : '1.0',
  namespace : 'MesonSub',
  symbol_prefix : 'meson_sub_',
  identifier_prefix : 'MesonSub',
  includes : ['GObject-2.0', 'Meson-1.0'],
  install : true
)

message('TEST: ' + girsubproject.outdir())

envdata = environment()
envdata.append('GI_TYPELIB_PATH', girsubproject.outdir(), 'subprojects/mesongir', separator : ':')
envdata.append('LD_LIBRARY_PATH', girsubproject.outdir(), 'subprojects/mesongir')
if ['windows', 'cygwin'].contains(host_machine.system())
  envdata.append('PATH', girsubproject.outdir(), 'subprojects/mesongir')
endif

test('gobject introspection/subproject/c', girexe)
test('gobject introspection/subproject/py', find_program('prog.py'),
     env : envdata)
