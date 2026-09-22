# Generate both header and source via template together.

myenums = gnome.mkenums('abc1',
  sources : 'meson-sample.h',
  h_template : 'enums.h.in',
  c_template : 'enums.c.in',
  install_header : true,
  install_dir : get_option('includedir'))

enums_c1 = myenums[0]
enums_h1 = myenums[1]

conf = configuration_data()
conf.set('ENUM_FILE', 'enums.h')
main = configure_file(
  input : 'main.c',
  output : 'main1.c',
  configuration : conf)

enumexe1 = executable('enumprog1', main, enums_c1, enums_h1,
dependencies : gobj)
test('enum test 1', enumexe1)

# Generate both header and source via template individually and overriding.

enums_h2 = gnome.mkenums('abc2',
  sources : 'meson-sample.h',
  h_template : 'enums2.h.in',
  ftail : '/* trailing header file info */',
  install_header : true,
  install_dir : get_option('includedir'))

enums_c2 = gnome.mkenums('abc2',
  sources : 'meson-sample.h',
  depends : [enums_h1, enums_h2],
  c_template : 'enums2.c.in',
  ftail : '/* trailing source file info */')
# explicitly don't set install_dir here, for bug testing
# See https://github.com/mesonbuild/meson/issues/9472

conf = configuration_data()
conf.set('ENUM_FILE', 'enums2.h')
main = configure_file(
  input : 'main.c',
  output : 'main2.c',
  configuration : conf)

enumexe2 = executable('enumprog2', main, enums_c2, enums_h2,
dependencies : gobj)
test('enum test 2', enumexe2)

# Generate both header and source by options only.
# These are specified in a way that should produce the same result as above
# (modulo any filename changes.)

enums_h3 = gnome.mkenums('enums3.h',
  sources : 'meson-sample.h',
  fhead : '''#ifndef MESON_ENUMS_H
#define MESON_ENUMS_H

#include <glib-object.h>

G_BEGIN_DECLS
''',
  fprod : '''
/* enumerations from "@basename@" */
''',
  vhead : '''GType @enum_name@_get_type(void) G_GNUC_CONST;
#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type())
''',
  ftail : '''
G_END_DECLS

#endif /* MESON_ENUMS_H */
''',
  install_header : true,
  install_dir : get_option('includedir'))

enums_c3 = gnome.mkenums('enums3.c',
  sources : 'meson-sample.h',
  depends : enums_h3,
  fhead : '''#include "enums3.h"
''',
  fprod : '''

/* enumerations from "@basename@" */
#include "@basename@"
''',
  vhead : '''
GType
@enum_name@_get_type(void) {
    static gsize static_g_define_type_id = 0;

    if(g_once_init_enter(&static_g_define_type_id)) {
        static const G@Type@Value values [] = {
''',
  vprod : '''            { @VALUENAME@, "@VALUENAME@", "@valuenick@" },''',
  vtail : '''            { 0, NULL, NULL }
        };

        GType g_define_type_id =
            g_@type@_register_static(g_intern_static_string("@EnumName@"), values);
        g_once_init_leave(&static_g_define_type_id, g_define_type_id);
    }

    return static_g_define_type_id;
}
''')

conf = configuration_data()
conf.set('ENUM_FILE', 'enums3.h')
main = configure_file(
  input : 'main.c',
  output : 'main3.c',
  configuration : conf)

enumexe3 = executable('enumprog3', main, enums_c3, enums_h3,
dependencies : gobj)
test('enum test 3', enumexe3)

enums4 = gnome.mkenums_simple('enums4', sources : files('meson-sample.h'),
                              function_prefix : '_')
enumexe4 = executable('enumprog4', 'main4.c', enums4, dependencies : gobj)

enums5 = gnome.mkenums_simple('enums5', sources : 'meson-sample.h',
                              install_header : true,
                              decorator : 'MESON_EXPORT',
                              header_prefix : '#include "meson-decls.h"')

conf = configuration_data()
conf.set('ENUM_FILE', 'enums5.h')
main = configure_file(
  input : 'main.c',
  output : 'main5.c',
  configuration : conf)

enumexe5 = executable('enumprog5', main, enums5, dependencies : gobj)

# Generate template then use as input to mkenums

# Simple trick to copy the file without substitutions, can be
# removed when https://github.com/mesonbuild/meson/pull/3383 is fixed
gen_h_template = configure_file(input: 'enums.h.in',
  output: 'enums6.h.in',
  configuration: configuration_data(),
  format: 'cmake')

enums_h6 = gnome.mkenums('enums6',
  sources : 'meson-sample.h',
  h_template : gen_h_template,
  ftail : '/* trailing header file info */',
  install_header : true,
  install_dir : get_option('includedir'))

conf = configuration_data()
conf.set('ENUM_FILE', 'enums6.h')
main = configure_file(
  input : 'main.c',
  output : 'main6.c',
  configuration : conf)

enumexe6 = executable('enumprog6', main, enums_c2, enums_h6,
dependencies : gobj)
test('enum test 4', enumexe6)

# Test with headers coming from other directories
# https://github.com/mesonbuild/meson/pull/10855
subdir('subdir')
enums7 = gnome.mkenums_simple('enums7', sources: ['meson-sample.h', h2, h3])
main = configure_file(
  input : 'main.c',
  output : 'mai7.c',
  configuration : {'ENUM_FILE': 'enums7.h'})
test('enums7 test', executable('enumprog7', main, enums7, dependencies : gobj))
