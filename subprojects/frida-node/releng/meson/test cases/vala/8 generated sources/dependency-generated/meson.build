# Test that dependencies with their own generated sources don't
# confuse the Vala build instruction generator.

# Test case for https://github.com/mesonbuild/meson/issues/1084

gnome = import('gnome')

gobject = dependency('gobject-2.0')

enums = gnome.mkenums('enum-types',
    sources: 'enums.h',
    c_template: 'enum-types.c.template',
    h_template: 'enum-types.h.template',
)

libcommon = library('common',
    enums[0], enums[1],
    dependencies: gobject)

common_dep = declare_dependency(
    # This is required so that whoever depends on this also depends
    # on the generated header; that won't happen implicitly.
    # See: https://github.com/mesonbuild/meson/issues/1084
    sources: enums[1],
    link_with: libcommon,
)

libplover_vala = library('plover',
    'lib.vala',
    dependencies: [common_dep, gobject]
)

plover_dep = declare_dependency(
    link_with: libplover_vala,
    dependencies: common_dep
)

vala_prog = executable('hello',
    'main.vala',
    link_with: libplover_vala,
    # There's no need to specify common_dep here since plover_dep pulls it
    # in, but it should be harmless to do so.
    dependencies: [common_dep, plover_dep, gobject]
)
