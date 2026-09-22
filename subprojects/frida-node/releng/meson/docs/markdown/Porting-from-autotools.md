# Porting from Autotools

This page uses
[AppStream-glib](https://github.com/hughsie/appstream-glib/) as an
example project. AppStream-Glib contains some libraries, GObject
Introspection data, tests, man pages, i18n, bash-completion with
optional flags to build/not build support for some things.

Meson comes with a helper script `ac_converter` that you can use to
convert the basic autoconf checks for your project.

## Configure.ac

First let's look at `configure.ac` and write the same in
`meson.build`.

```autoconf
AC_PREREQ(2.63)
```
Meson doesn't provide the same function, so just ignore this.

### Defining variables
`configure.ac`:
```autoconf
m4_define([as_major_version], [0])
m4_define([as_minor_version], [3])
m4_define([as_micro_version], [6])
m4_define([as_version],
          [as_major_version.as_minor_version.as_micro_version])
```
`meson.build`:
```meson

as_version = meson.project_version() # set in project() below
ver_arr = as_version.split('.')

as_major_version = ver_arr[0]
as_minor_version = ver_arr[1]
as_micro_version = ver_arr[2]
```

### Initializing project and setting compilers
`configure.ac`:
```autoconf
AC_INIT([appstream-glib],[as_version])
AC_PROG_CC
```
`meson.build`:
```meson
project('appstream-glib', 'c', version : '0.3.6')
```
Note that this must be the first line of your `meson.build` file.

### AC_SUBST
`configure.ac`:
```autoconf
AC_SUBST(AS_MAJOR_VERSION)
AC_SUBST(AS_MINOR_VERSION)
AC_SUBST(AS_MICRO_VERSION)
AC_SUBST(AS_VERSION)
```

You don't need to do the same in Meson, because it does not have two
different types of files (Makefile, configure).

### Auto headers

`configure.ac`:

```autoconf
AC_CONFIG_HEADERS([config.h])
```

`meson.build`:

```meson
conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', as_version)
configure_file(output : 'config.h',
               configuration : conf)
```

Meson doesn't support autoheaders, you need to manually specify what
do you want to see in header file, write `configuration_data()` object
and use `configure_file()`.

You can also substitute variables of type `@SOME_VAR@` with configure
data. The details are on the [configuration page](Configuration.md).

### Finding programs

`configure.ac`:

```autoconf
AC_PATH_PROG(GPERF, [gperf], [no])
if test x$GPERF != xno ; then
        AC_DEFINE(HAVE_GPERF,[1], [Use gperf])
fi
AM_CONDITIONAL(HAVE_GPERF, [test x$GPERF != xno])
```

`meson.build`:

```meson
gperf = find_program('gperf', required : false)
if gperf.found()
  conf.set('HAVE_GPERF', 1)
endif
```

### Finding pkg-config modules

`configure.ac`:

```autoconf
PKG_CHECK_MODULES(SOUP, libsoup-2.4 >= 2.24)
```

`meson.build`:

```meson
soup = dependency('libsoup-2.4', version : '>= 2.24')
```

### Arguments

`configure.ac`:

```autoconf
AC_ARG_ENABLE(dep11, AS_HELP_STRING([--enable-dep11],[enable DEP-11]),
              enable_dep11=$enableval,enable_dep11=yes)
AM_CONDITIONAL(HAVE_DEP11, test x$enable_dep11 = xyes)
if test x$enable_dep11 = xyes; then
  AC_CHECK_HEADER(yaml.h, [], [AC_MSG_ERROR([No yaml.h])])
  YAML_LIBS="-lyaml"
  AC_SUBST(YAML_LIBS)
  AC_DEFINE(AS_BUILD_DEP11,1,[Build DEP-11 code])
fi
```

`meson.build`:

```meson
if get_option('enable-dep11')
  yaml = dependency('yaml-0.1')
  conf.set('AS_BUILD_DEP11', 1)
else
  yaml = dependency('yaml-0.1', required : false)
endif
```

`meson.options`:

```meson
option('enable-dep11', type : 'boolean', value : true, description : 'enable DEP-11')
```

## Makefile.am

Next step is `Makefile.am`. In Meson you don't need to have other
file, you still use `meson.build`.

### Sub directories

`Makefile.am`:

```make
SUBDIRS =                                         \
        libappstream-glib
```

`meson.build`:

```meson
subdir('libappstream-glib')
```

### *CLEANFILES, EXTRA_DIST, etc.

`Makefile.am`:

```make
DISTCLEANFILES =                                        \
        appstream-glib-*.tar.xz

MAINTAINERCLEANFILES =                                  \
        *~                                              \
        ABOUT-NLS                                       \
        aclocal.m4                                      \
        ChangeLog                                       \
        compile                                         \
        config.guess                                    \
        config.h.*                                      \
        config.rpath

EXTRA_DIST =                                            \
        COPYING                                         \
        MAINTAINERS                                     \
        AUTHORS                                         \
        README.md                                       \
        NEWS                                            \
        autogen.sh                                      \
        config.h
```

In Meson you don't need have `*CLEANFILES`, because in Meson you are
building in temporary directory (usually called `build`), you manually
removing it. You also not need to use `EXTRA_DIST`, because you will
make tarballs via `git archive` or something like this.

### glib-compile-resources

`Makefile.am`:
```make
as-resources.c: appstream-glib.gresource.xml                    \
                as-stock-icons.txt                              \
                as-license-ids.txt                              \
                as-blacklist-ids.txt                            \
                as-category-ids.txt                             \
                as-environment-ids.txt
        $(AM_V_GEN)                                             \
        glib-compile-resources                                  \
                --sourcedir=$(srcdir)                           \
                --sourcedir=$(top_builddir)/data                \
                --target=$@                                     \
                --generate-source                               \
                --c-name as                                     \
                $(srcdir)/appstream-glib.gresource.xml
as-resources.h: appstream-glib.gresource.xml                    \
                as-stock-icons.txt                              \
                as-license-ids.txt                              \
                as-blacklist-ids.txt                            \
                as-category-ids.txt                             \
                as-environment-ids.txt
        $(AM_V_GEN)                                             \
        glib-compile-resources                                  \
                --sourcedir=$(srcdir)                           \
                --sourcedir=$(top_builddir)/data                \
                --target=$@                                     \
                --generate-header                               \
                --c-name as                                     \
                $(srcdir)/appstream-glib.gresource.xml

BUILT_SOURCES =                                                 \
        as-resources.c                                          \
        as-resources.h
```

`meson.build`:

```meson
asresources = gnome.compile_resources(
  'as-resources', 'appstream-glib.gresource.xml',
  source_dir : '.',
  c_name : 'as')
```

### Headers

`Makefile.am`:

```make
libappstream_glib_includedir = $(includedir)/libappstream-glib
libappstream_glib_include_HEADERS =                             \
        appstream-glib.h                                        \
        as-app.h                                                \
        as-bundle.h                                             \
        as-enums.h                                              \
        as-icon.h                                               \
        as-image.h                                              \
        as-inf.h                                                \
        as-node.h                                               \
        as-problem.h                                            \
        as-provide.h                                            \
        as-release.h                                            \
        as-screenshot.h                                         \
        as-store.h                                              \
        as-tag.h                                                \
        as-utils.h                                              \
        as-version.h
```

`meson.build`:

```meson
headers = [
  'appstream-glib.h',
  'as-app.h',
  'as-bundle.h',
  'as-enums.h',
  'as-icon.h',
  'as-image.h',
  'as-inf.h',
  'as-node.h',
  'as-problem.h',
  'as-provide.h',
  'as-release.h',
  'as-screenshot.h',
  'as-store.h',
  'as-tag.h',
  'as-utils.h',
  'as-version.h']
install_headers(headers, subdir : 'libappstream-glib')
```

### Libraries

`Makefile.am`:
```make
lib_LTLIBRARIES =                                               \
        libappstream-glib.la
libappstream_glib_la_SOURCES =                                  \
        as-app.c                                                \
        as-app-desktop.c                                        \
        as-app-inf.c                                            \
        as-app-private.h                                        \
        as-app-validate.c                                       \
        as-bundle.c                                             \
        as-bundle-private.h                                     \
        as-cleanup.h                                            \
        as-enums.c                                              \
        as-icon.c                                               \
        as-icon-private.h                                       \
        as-image.c                                              \
        as-image-private.h                                      \
        as-inf.c                                                \
        as-inf.h                                                \
        as-node.c                                               \
        as-node-private.h                                       \
        as-problem.c                                            \
        as-problem.h                                            \
        as-provide.c                                            \
        as-provide-private.h                                    \
        as-release.c                                            \
        as-release-private.h                                    \
        as-resources.c                                          \
        as-resources.h                                          \
        as-screenshot.c                                         \
        as-screenshot-private.h                                 \
        as-store.c                                              \
        as-tag.c                                                \
        as-utils.c                                              \
        as-utils-private.h                                      \
        as-version.h                                            \
        as-yaml.c                                               \
        as-yaml.h

libappstream_glib_la_LIBADD =                                   \
        $(GLIB_LIBS)                                            \
        $(GDKPIXBUF_LIBS)                                       \
        $(LIBARCHIVE_LIBS)                                      \
        $(SOUP_LIBS)                                            \
        $(YAML_LIBS)

libappstream_glib_la_LDFLAGS =                                  \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
        -export-dynamic                                         \
        -no-undefined                                           \
        -export-symbols-regex '^as_.*'
```

`meson.build`:

```meson
sources = [
  'as-app.c',
  'as-app-desktop.c',
  'as-app-inf.c',
  'as-app-private.h',
  'as-app-validate.c',
  'as-bundle.c',
  'as-bundle-private.h',
  'as-cleanup.h',
  'as-enums.c',
  'as-icon.c',
  'as-icon-private.h',
  'as-image.c',
  'as-image-private.h',
  'as-inf.c',
  'as-inf.h',
  'as-node.c',
  'as-node-private.h',
  'as-problem.c',
  'as-problem.h',
  'as-provide.c',
  'as-provide-private.h',
  'as-release.c',
  'as-release-private.h',
  asresources,
  'as-screenshot.c',
  'as-screenshot-private.h',
  'as-store.c',
  'as-tag.c',
  'as-utils.c',
  'as-utils-private.h',
  'as-version.h',
  'as-yaml.c',
  'as-yaml.h']

deps = [glib, gdkpixbuf, libarchive, soup, yaml]

mapfile = 'appstream-glib.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
asglib = shared_library(
  'appstream-glib', sources,
  soversion : lt_current,
  version : lt_version,
  dependencies : deps,
  include_directories : include_directories('@0@/..'.format(meson.current_build_dir())),
  link_args : ['-Wl,--no-undefined', vflag],
  link_depends : mapfile,
  install : true)
```

`appstream-glib.map`:

```
{
global:
    as_*;
local:
    *;
};
```

### Custom targets

`Makefile.am`:

```make
if HAVE_GPERF
as-tag-private.h: as-tag.gperf
        $(AM_V_GEN) gperf < $< > $@

libappstream_glib_la_SOURCES += as-tag-private.h
BUILT_SOURCES += as-tag-private.h
endif
```

`meson.build`:

```meson
if gperf.found()
  astagpriv = custom_target('gperf as-tag',
                            output : 'as-tag-private.h',
                            input : 'as-tag.gperf',
                            command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])
  sources = sources + [astagpriv]
endif
```

### Global CFLAGS

`Makefile.am`:

```make
AM_CPPFLAGS =                                                   \
        -DAS_COMPILATION                                        \
        -DLOCALSTATEDIR=\""$(localstatedir)"\"                  \
        -DG_LOG_DOMAIN=\"As\"
```

`meson.build`:

```meson
add_project_arguments('-DG_LOG_DOMAIN="As"', language : 'c')
add_project_arguments('-DAS_COMPILATION', language : 'c')
add_project_arguments('-DLOCALSTATEDIR="/var"', language : 'c')
```

### Tests

`Makefile.am`:

```make
check_PROGRAMS =                                                \
        as-self-test
as_self_test_SOURCES =                                          \
        as-self-test.c
as_self_test_LDADD =                                            \
        $(GLIB_LIBS)                                            \
        $(GDKPIXBUF_LIBS)                                       \
        $(LIBARCHIVE_LIBS)                                      \
        $(SOUP_LIBS)                                            \
        $(YAML_LIBS)                                            \
        $(lib_LTLIBRARIES)
as_self_test_CFLAGS = -DTESTDATADIR=\""$(top_srcdir)/data/tests"\"

TESTS = as-self-test
```

`meson.build`:

```meson
selftest = executable(
  'as-self-test', 'as-self-test.c',
  include_directories : include_directories('@0@/..'.format(meson.current_build_dir())),
  dependencies : deps,
  c_args : '-DTESTDATADIR="@0@/../data/tests"'.format(meson.current_source_dir()),
  link_with : asglib)
test('as-self-test', selftest)
```

### GObject Introspection

`Makefile.am`:

```make
introspection_sources =                                         \
        as-app.c                                                \
        as-app-validate.c                                       \
        as-app.h                                                \
        as-bundle.c                                             \
        as-bundle.h                                             \
        as-enums.c                                              \
        as-enums.h                                              \
        as-icon.c                                               \
        as-icon.h                                               \
        as-image.c                                              \
        as-image.h                                              \
        as-inf.c                                                \
        as-inf.h                                                \
        as-node.c                                               \
        as-node.h                                               \
        as-problem.c                                            \
        as-problem.h                                            \
        as-provide.c                                            \
        as-provide.h                                            \
        as-release.c                                            \
        as-release.h                                            \
        as-screenshot.c                                         \
        as-screenshot.h                                         \
        as-store.c                                              \
        as-store.h                                              \
        as-tag.c                                                \
        as-tag.h                                                \
        as-utils.c                                              \
        as-utils.h                                              \
        as-version.h

AppStreamGlib-1.0.gir: libappstream-glib.la
AppStreamGlib_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0 GdkPixbuf-2.0
AppStreamGlib_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
AppStreamGlib_1_0_gir_SCANNERFLAGS = --identifier-prefix=As \
                                --symbol-prefix=as_ \
                                --warn-all \
                                --add-include-path=$(srcdir)
AppStreamGlib_1_0_gir_EXPORT_PACKAGES = appstream-glib
AppStreamGlib_1_0_gir_LIBS = libappstream-glib.la
AppStreamGlib_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += AppStreamGlib-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
```

`meson.build`:

```meson
introspection_sources = [
  'as-app.c',
  'as-app-validate.c',
  'as-app.h',
  'as-bundle.c',
  'as-bundle.h',
  'as-enums.c',
  'as-enums.h',
  'as-icon.c',
  'as-icon.h',
  'as-image.c',
  'as-image.h',
  'as-inf.c',
  'as-inf.h',
  'as-node.c',
  'as-node.h',
  'as-problem.c',
  'as-problem.h',
  'as-provide.c',
  'as-provide.h',
  'as-release.c',
  'as-release.h',
  'as-screenshot.c',
  'as-screenshot.h',
  'as-store.c',
  'as-store.h',
  'as-tag.c',
  'as-tag.h',
  'as-utils.c',
  'as-utils.h',
  'as-version.h']

gnome.generate_gir(asglib,
  sources : introspection_sources,
  nsversion : '1.0',
  namespace : 'AppStreamGlib',
  symbol_prefix : 'as_',
  identifier_prefix : 'As',
  export_packages : 'appstream-glib',
  includes : ['GObject-2.0', 'Gio-2.0', 'GdkPixbuf-2.0'],
  install : true
)
```

### GSettings

`configure.ac`:
```sh
GLIB_GSETTINGS
```

`Makefile.am`:
```make
gsettings_SCHEMAS = foo.gschema.xml
@GSETTINGS_RULES@
```

`meson.build`:
```meson
install_data('foo.gschema.xml', install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')
meson.add_install_script('meson_post_install.py')
```

`meson_post_install.py`:
```python
#!/usr/bin/env python3

import os
import subprocess

schemadir = os.path.join(os.environ['MESON_INSTALL_PREFIX'], 'share', 'glib-2.0', 'schemas')

if not os.environ.get('DESTDIR'):
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
```

### gettext

Note this example does not include `intltool` usage.

`configure.ac`:
```m4
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])

GETTEXT_PACKAGE=foo
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The prefix for our gettext translation domains.])
```

`po/Makevars`:
```make
XGETTEXT_OPTIONS =  --from-code=UTF-8 --keyword=_ --keyword=N_ --keyword=C_:1c,2 --keyword=NC_:1c,2 --keyword=g_dngettext:2,3 --add-comments
```

`Makefile.am`:
```make
%.desktop: %.desktop.in
    $(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

%.appdata.xml: %.appdata.xml.in
    $(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
```

`meson.build`:
```meson
i18n = import('i18n')

gettext_package = 'foo'
add_project_arguments('-DGETTEXT_PACKAGE=' + gettext_package, language: 'c')
subdir('po')

i18n.merge_file(
  input: 'foo.desktop.in',
  output: 'foo.desktop',
  type: 'desktop',
  po_dir: 'po',
  install: true,
  install_dir: get_option('datadir') / 'applications'
)

i18n.merge_file(
  input: 'foo.appdata.xml.in',
  output: 'foo.appdata.xml',
  po_dir: 'po',
  install: true,
  install_dir: get_option('datadir') / 'appdata'
)
```

`po/meson.build`:
```meson
i18n.gettext(gettext_package, preset: 'glib')
```
