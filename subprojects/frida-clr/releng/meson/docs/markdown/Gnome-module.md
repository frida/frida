# GNOME module

This module provides helper tools for build operations needed when
building Gnome/GLib programs.

**Note**: the compilation commands here might not work properly when
  you change the source files. This is a bug in the respective
  compilers which do not expose the required dependency
  information. This has been reported upstream in [this bug]. Until
  this is fixed you need to be careful when changing your source
  files.

  [this bug]: https://bugzilla.gnome.org/show_bug.cgi?id=745754

## Usage

To use this module, just do: **`gnome = import('gnome')`**. The
following functions will then be available as methods on the object
with the name `gnome`. You can, of course, replace the name `gnome`
with anything else.

### gnome.compile_resources()

```
    gnome.compile_resources(id: string, input_file: string | File,
                            build_by_default: bool = false,
                            c_name: string | None = None,
                            dependencies: [](File, CustomTarget, CustomTargetIndex) = [],
                            export: bool = false,
                            extra_args: []string = [],
                            gresource_bundle: bool = false,
                            install_dir: string | None = None,
                            source_dir: [string] = [],
                            ): (CustomTarget, CustomTarget) | CustomTarget
```

This function compiles resources specified in an XML file into code
that can be embedded inside the main binary. Similar a build target it
takes two positional arguments. The first one is the name of the
resource and the second is the XML file containing the resource
definitions. If the name is `foobar`, Meson will generate a header
file called `foobar.h`, which you can then include in your sources.
The resources specified are automatically added as dependencies of the
generated target.

* `c_name`: passed to the resource compiler as an argument after
  `--c-name`
* `dependencies`: extra targets to depend upon for building
* `export`: (*Added 0.37.0*) if true, export the symbols of the
  generated sources
* `extra_args`: extra command line arguments to pass to the resource
* `gresource_bundle`: (*Added 0.37.0*) if true, output a `.gresource`
  file instead of source
* `install`: (*Added 0.37.0*) if true, install the gresource file
* `install_dir`: (*Added 0.37.0*) location to install the header or
  bundle depending on previous options
* `install_header`: (*Added 0.37.0*) if true, install the header file
* `source_dir`: a list of directories where the resource compiler
  should look up the files

Returns an array containing: `[c_source, header_file]` or
`[gresource_bundle]`

Example:

```meson
gnome = import('gnome')

asresources = gnome.compile_resources(
    'as-resources', 'data/asresources.gresource.xml',
    source_dir: 'data',
    c_name: 'as'
)

executable(
    meson.project_name(),
    asresources,
    dependencies: my_deps,
    install: true
)
```

### gnome.generate_gir()

Generates GObject introspection data.

Takes one or more positional arguments:

Either one or more library objects you want to build gir data for, or a single
executable object.

There are several keyword arguments. Many of these map directly to the
`g-ir-scanner` tool so see its documentation for more information.

* `dependencies`: deps to use during introspection scanning
* `extra_args`: command line arguments to pass to gir compiler
* `env`: (*Added 1.2.0*) environment variables to set, such as
  `{'NAME1': 'value1', 'NAME2': 'value2'}` or `['NAME1=value1', 'NAME2=value2']`,
  or an [[@env]] object which allows more sophisticated environment juggling.
* `export_packages`: extra packages the gir file exports
* `sources`: the list of sources to be scanned for gir data
* `nsversion`: namespace version
* `namespace`: the namespace for this gir object which determines
  output files
* `identifier_prefix`: the identifier prefix for the gir object,
  e.g. `Gtk`
* `includes`: list of gir names to be included, can also be a GirTarget
* `header`: *(Added 0.43.0)* name of main c header to include for the library, e.g. `glib.h`
* `include_directories`: extra include paths to look for gir files
* `install`: if true, install the generated files
* `install_gir`: (*Added 0.61.0*) overrides `install`, whether to install the
  generated gir
* `install_dir_gir`: (*Added 0.35.0*) which directory to install the
  gir file into; (*Deprecated since 0.61.0*) can be false to disable installation
* `install_typelib`: (*Added 0.61.0*) overrides `install`, whether to install the
  generated typelib
* `install_dir_typelib`: (*Added 0.35.0*) which directory to install
  the typelib file into; (*Deprecated since 0.61.0*) can be false to disable installation
* `link_with`: list of libraries to link with
* `symbol_prefix`: the symbol prefix for the gir object, e.g. `gtk`,
  (*Since 0.43.0*) an ordered list of multiple prefixes is allowed
* `fatal_warnings`: *Since 0.55.0* turn scanner warnings into fatal errors.

Returns an array of two elements which are: `[gir_target,
typelib_target]`

### gnome.genmarshal()

Generates a marshal file using the `glib-genmarshal` tool. The first
argument is the basename of the output files.

* `depends` [](BuildTarget | CustomTarget | CustomTargetIndex):
  passed directly to CustomTarget (*since 0.61.0*)
* `depend_files` [](str | File): Passed directly to CustomTarget (*since 0.61.0*)
* `extra_args`: (*Added 0.42.0*) additional command line arguments to pass
* `install_dir`: directory to install header to
* `install_header`: if true, install the generated header
* `install_dir`: directory to install header to
* `install_header`: if true, install the generated header
* `internal`: if true, mark generated sources as internal to `glib-genmarshal`
  (*Requires GLib 2.54*)
* `nostdinc`: if true, don't include the standard marshallers from glib
* `prefix`: the prefix to use for symbols
* `skip_source`: if true, skip source location comments
* `sources` [](str | File) *required*: the list of sources to use as inputs
* `stdinc`: if true, include the standard marshallers from glib
* `valist_marshallers`: if true, generate va_list marshallers

*Added 0.35.0*

Returns an array of two elements which are: `[c_source, header_file]`

### gnome.mkenums()

Generates enum files for GObject using the `glib-mkenums` tool. The
first argument is the base name of the output files, unless
`c_template` and `h_template` are specified. In this case, the output
files will be the base name of the values passed as templates.

This method is essentially a wrapper around the `glib-mkenums` tool's
command line API. It is the most featureful method for enum creation.

Typically you either provide template files or you specify the various
template sections manually as strings.

Most libraries and applications will be using the same standard
template with only minor tweaks, in which case the
`gnome.mkenums_simple()` convenience method can be used instead.

Note that if you `#include` the generated header in any of the sources
for a build target, you must add the generated header to the build
target's list of sources to codify the dependency. This is true for

all generated sources, not just `mkenums`.

* `c_template`: template to use for generating the source
* `comments`: comment passed to the command
* `h_template`: template to use for generating the header
* `identifier_prefix`: prefix to use for the identifiers
* `install_header`: if true, install the generated header
* `install_dir`: directory to install the header
* `sources`: the list of sources to make enums with
* `symbol_prefix`: prefix to use for the symbols
* `eprod`: enum text
* `fhead`: file header
* `fprod`: file text
* `ftail`: file tail
* `vhead`: value text
* `vtail`: value tail

*Added 0.35.0*

Returns an array of two elements which are: `[c_source, header_file]`

### gnome.mkenums_simple()

Generates enum `.c` and `.h` files for GObject using the
`glib-mkenums` tool with the standard template used by most
GObject-based C libraries. The first argument is the base name of the
output files.

Note that if you `#include` the generated header in any of the sources
for a build target, you must add the generated header to the build
target's list of sources to codify the dependency. This is true for
all generated sources, not just `mkenums_simple`.

The generated source file includes all headers passed to the sources keyword
argument, using paths relative to current build or source directory. That means
that targets that compile the generated source file must have the current
directory in its `include_directories`. *Since 1.3.0* `sources` outside of
current directory do not require adding those directories into
`include_directories` anymore.

* `body_prefix`: additional prefix at the top of the body file,
  e.g. for extra includes
* `decorator`: optional decorator for the function declarations,
  e.g. `GTK_AVAILABLE` or `GST_EXPORT`
* `function_prefix`: additional prefix for function names, e.g. in
  case you want to add a leading underscore to functions used only
  internally
* `header_prefix`: additional prefix at the top of the header file,
  e.g. for extra includes (which may be needed if you specify a
  decorator for the function declarations)
* `install_header`: if true, install the generated header
* `install_dir`: directory to install the header
* `identifier_prefix`: prefix to use for the identifiers
* `sources`: the list of sources to make enums with
* `symbol_prefix`: prefix to use for the symbols

Example:

```meson
gnome = import('gnome')

my_headers = ['myheader1.h', 'myheader2.h']
my_sources = ['mysource1.c', 'mysource2.c']

# will generate myenums.c and myenums.h based on enums in myheader1.h and myheader2.h
enums = gnome.mkenums_simple('myenums', sources : my_headers)

mylib = library('my', my_sources, enums,
                include_directories: my_incs,
                dependencies: my_deps,
                c_args: my_cargs,
                install: true)
```

*Added 0.42.0*

Returns an array of two elements which are: `[c_source, header_file]`

### gnome.compile_schemas()

When called, this method will compile the gschemas in the current
directory. Note that this is not for installing schemas and is only
useful when running the application locally for example during tests.

* `build_by_default`: causes, when set to true, to have this target be
  built by default, that is, when invoking plain `meson compile`, the default
  value is true for all built target types
* `depend_files`: files ([[@str]],
  [[files]], or
  [[configure_file]]) of
  schema source XML files that should trigger a re-compile if changed.

### gnome.gdbus_codegen()

Compiles the given XML schema into gdbus source code. Takes two
positional arguments, the first one specifies the base name to use
while creating the output source and header and the second specifies
one XML file.

* `sources`: list of XML files
* `interface_prefix`: prefix for the interface
* `namespace`: namespace of the interface
* `extra_args`: (*Added 0.47.0*) additional command line arguments to pass
* `autocleanup`: *(Added 0.47.0)* if set generates autocleanup code. Can be one of `none`, `objects` or `all`
* `object_manager`: *(Added 0.40.0)* if true generates object manager code
* `annotations`: *(Added 0.43.0)* list of lists of 3 strings for the annotation for `'ELEMENT', 'KEY', 'VALUE'`
* `docbook`: *(Added 0.43.0)* prefix to generate `'PREFIX'-NAME.xml` docbooks
* `build_by_default`: causes, when set to true, to have this target be
  built by default, that is, when invoking plain `meson compile`, the default
  value is true for all built target types
* `install_dir`: (*Added 0.46.0*) location to install the header or
  bundle depending on previous options
* `install_header`: (*Added 0.46.0*) if true, install the header file

Starting *0.46.0*, this function returns a list of at least two custom
targets (in order): one for the source code and one for the header.
The list will contain a third custom target for the generated docbook
files if that keyword argument is passed.

Earlier versions return a single custom target representing all the
outputs. Generally, you should just add this list of targets to a top
level target's source list.

Example:

```meson
gnome = import('gnome')

# The returned source would be passed to another target
gdbus_src = gnome.gdbus_codegen('example-interface',
  sources: 'com.example.Sample.xml',
  interface_prefix : 'com.example.',
  namespace : 'Sample',
  annotations : [
    ['com.example.Hello()', 'org.freedesktop.DBus.Deprecated', 'true']
  ],
  docbook : 'example-interface-doc'
)
```

### gnome.generate_vapi()

Creates a VAPI file from gir. The first argument is the name of the
library.

* `gir_dirs`: extra directories to include for gir files
* `install`: if true, install the VAPI file
* `install_dir`: location to install the VAPI file (defaults to datadir/vala/vapi)
* `metadata_dirs`: extra directories to include for metadata files
* `packages`: VAPI packages that are depended upon
* `sources`: the gir source to generate the VAPI from
* `vapi_dirs`: extra directories to include for VAPI files

Returns a custom dependency that can be included when building other
VAPI or Vala binaries.

*Added 0.36.0*

### gnome.yelp()

```meson
  gnome.yelp(id: string, sources: ...string, sources: []string, media: []string,
             languages: []string, symlink_media: bool = true): void
```

Installs help documentation for Yelp using itstool and gettext. The first
argument is the project id.

Additionally, sources can be passed as additional positional arguments. This
was, however, undocumented and never officially supported. Due to a longstanding
bug, passing sources as a keyword argument will result in the positional
argument sources to be ignored. *since 0.60.0* A warning is raised in this case.

*Since 0.43.0* if "languages" is not specified, a
[LINGUAS](https://www.gnu.org/software/gettext/manual/html_node/po_002fLINGUAS.html)
file will be read instead.

*Since 0.60.0* the use of the positional argument sources has been deprecated,
and the "sources" keyword argument should be used instead. The passing of
sources as positional arguments will be removed in the future.

This also creates two targets for translations
`help-$project-update-po` and `help-$project-pot`.

* `languages`: *(deprecated since 0.43.0)* list of languages for translation, overrides the LINGUAS file
* `media`: list of media such as images
* `sources`: list of pages
* `symlink_media`: if media should be symlinked not copied (defaults to `true` since 0.42.0)

Note that very old versions of yelp may not support symlinked media;
At least 3.10 should work.

*Added 0.36.0*

### gnome.gtkdoc()

Compiles and installs gtkdoc documentation into
`prefix/share/gtk-doc/html`. Takes one positional argument: The name
of the module.

* `content_files`: a list of content files
* `dependencies`: a list of dependencies
* `fixxref_args`: a list of arguments to pass to `gtkdoc-fixxref`
* `gobject_typesfile`: a list of type files
* `include_directories`: extra include paths to pass to `gtkdoc-scangobj`
* `ignore_headers`: a list of header files to ignore
* `html_assets`: a list of assets for the HTML pages
* `html_args` a list of arguments to pass to `gtkdoc-mkhtml`
* `install`: if true, installs the generated docs
* `install_dir`: the directory to install the generated docs relative
  to the gtk-doc html dir or an absolute path (default: module name)
* `main_xml`: specifies the main XML file
* `main_sgml`: equal to `main_xml`
* `mkdb_args`: a list of arguments to pass to `gtkdoc-mkdb`
* `namespace`: specifies the name space to pass to `gtkdoc-mkdb`
* `module_version`: the version of the module, affects the installed location and the devhelp2 file location
* `scan_args`: a list of arguments to pass to `gtkdoc-scan`
* `scanobjs_args`: a list of arguments to pass to `gtkdoc-scangobj`
* `c_args`: (*Added 0.48.0*) additional compile arguments to pass
* `src_dir`: include_directories to include
* `check`: (*Since 0.52.0*) if `true` runs `gtkdoc-check` when running unit tests.
  Note that this has the downside of rebuilding the doc for each build, which is
  often very slow. It usually should be enabled only in CI.

This also creates a `$module-doc` target that can be run to build
documentation. Normally the documentation is only built on install.

*Since 0.52.0* Returns a target object that can be passed as
dependency to other targets using generated doc files (e.g. in
`content_files` of another doc).

### gnome.gtkdoc_html_dir()

Takes as argument a module name and returns the path where that
module's HTML files will be installed. Usually used with
`install_data` to install extra files, such as images, to the output
directory.

### gnome.post_install()

*Since 0.57.0*

Post-install update of various system wide caches. Each script will be executed
only once even if `gnome.post_install()` is called multiple times from multiple
subprojects. If `DESTDIR` is specified during installation all scripts will be
skipped.

It takes the following keyword arguments:
- `glib_compile_schemas`: If set to `true`, update `gschemas.compiled` file in
  `<prefix>/<datadir>/glib-2.0/schemas`.
- `gio_querymodules`: List of directories relative to `prefix` where
  `giomodule.cache` file will be updated.
- `gtk_update_icon_cache`: If set to `true`, update `icon-theme.cache` file in
  `<prefix>/<datadir>/icons/hicolor`.
- `update_desktop_database`: *Since 0.59.0* If set to `true`, update cache of
  MIME types handled by desktop files in `<prefix>/<datadir>/applications`.
- `update_mime_database`: *Since 0.64.0* If set to `true`, update cache of
  MIME types in `<prefix>/<datadir>/mime`.
