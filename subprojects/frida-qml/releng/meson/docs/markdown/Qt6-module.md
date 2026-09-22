# Qt6 module

*New in Meson 0.57.0*

The Qt6 module provides methods to automatically deal with the various
tools and steps required for Qt.

<div class="alert alert-warning">
<strong>Warning:</strong> before version 0.63.0 Meson would fail to find
Qt 6.1 or later due to the Qt tools having moved to the libexec subdirectory,
and tool names being suffixed with only the Qt major version number e.g. qmake6.
</div>

## compile_resources

*New in 0.59.0*

Compiles Qt's resources collection files (.qrc) into c++ files for compilation.

It takes no positional arguments, and the following keyword arguments:
  - `name` (string | empty): if provided a single .cpp file will be generated,
    and the output of all qrc files will be combined in this file, otherwise
    each qrc file be written to it's own cpp file.
  - `sources` (File | string | custom_target | custom_target index | generator_output)[]:
    A list of sources to be transpiled. Required, must have at least one source<br/>
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-rcc`
  - `method` string: The method to use to detect Qt, see [[dependency]]

## compile_ui

*New in 0.59.0*

Compiles Qt's ui files (.ui) into header files.

It takes no positional arguments, and the following keyword arguments:
  - `sources` (File | string | custom_target | custom_target index | generator_output)[]:
    A list of sources to be transpiled. Required, must have at least one source<br/>
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-uic`
  - `method` string: The method to use to detect Qt, see [[dependency]]
  - `preserve_paths` bool: *New in 1.4.0*. If `true`, specifies that the output
    files need to maintain their directory structure inside the target temporary
    directory. For instance, when a file called `subdir/one.input` is processed
    it generates a file `{target private directory}/subdir/one.out` when `true`,
    and `{target private directory}/one.out` when `false` (default).

## compile_moc

*New in 0.59.0*

Compiles Qt's moc files (.moc) into header and/or source files. At least one of
the keyword arguments `headers` and `sources` must be provided.

It takes no positional arguments, and the following keyword arguments:
  - `sources` (File | string | custom_target | custom_target index | generator_output)[]:
    A list of sources to be transpiled into .moc files for manual inclusion.<br/>
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `headers` (File | string | custom_target | custom_target index | generator_output)[]:
     A list of headers to be transpiled into .cpp files<br/>
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-moc`
  - `method` string: The method to use to detect Qt, see [[dependency]]
  - `dependencies`: dependency objects whose include directories are used by moc.
  - `include_directories` (string | IncludeDirectory)[]: A list of `include_directory()`
    objects used when transpiling the .moc files
  - `preserve_paths` bool: *New in 1.4.0*. If `true`, specifies that the output
    files need to maintain their directory structure inside the target temporary
    directory. For instance, when a file called `subdir/one.input` is processed
    it generates a file `{target private directory}/subdir/one.out` when `true`,
    and `{target private directory}/one.out` when `false` (default).

## preprocess

Consider using `compile_resources`, `compile_ui`, and `compile_moc` instead.

Takes sources for moc, uic, and rcc, and converts them into c++ files for
compilation.

Has the following signature: `qt.preprocess(name: str | None, *sources: str)`

If the `name` parameter is passed then all of the rcc files will be written to
a single output file

*Deprecated in 0.59.0*: Files given in the variadic `sources` arguments as well
as the `sources` keyword argument, were passed unmodified through the preprocessor
programs. Don't do this - just add the output of `preprocess()` to another sources
list:
```meson
sources = files('a.cpp', 'main.cpp', 'bar.c')
sources += qt.preprocess(qresources : ['resources'])
```

This method takes the following keyword arguments:
 - `qresources` (string | File)[]: Passed to the RCC compiler
 - `ui_files`: (string | File | CustomTarget)[]: Passed the `uic` compiler
 - `moc_sources`: (string | File | CustomTarget)[]: Passed the `moc` compiler.
   These are converted into .moc files meant to be `#include`ed
 - `moc_headers`: (string | File | CustomTarget)[]: Passed the `moc` compiler.
   These will be converted into .cpp files
 - `include_directories` (IncludeDirectories | string)[], the directories to add
   to header search path for `moc`
 - `moc_extra_arguments` string[]: any additional arguments to `moc`.
 - `uic_extra_arguments` string[]: any additional arguments to `uic`.
 - `rcc_extra_arguments` string[]: any additional arguments to `rcc`.
 - `dependencies` Dependency[]: dependency objects needed by moc.
 - *Deprecated in 0.59.0.*: `sources`: a list of extra sources, which are added
   to the output unchanged.
 - `preserve_paths` bool: *Since 1.4.0*. If `true`, specifies that the output
   files need to maintain their directory structure inside the target temporary
   directory. For instance, when a file called `subdir/one.input` is processed
   it generates a file `{target private directory}/subdir/one.out` when `true`,
   and `{target private directory}/one.out` when `false` (default).
    
It returns an array of targets and sources to pass to a compilation target.

## compile_translations

This method generates the necessary targets to build translation files with
lrelease, it takes no positional arguments, and the following keyword arguments:

 - `ts_files` (File | string | custom_target | custom_target index | generator_output)[]:
    the list of input translation files produced by Qt's lupdate tool.<br/>
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
 - `install` bool: when true, this target is installed during the install step (optional).
 - `install_dir` string: directory to install to (optional).
 - `build_by_default` bool: when set to true, to have this target be built by
   default, that is, when invoking `meson compile`; the default value is false
   (optional).
 - `qresource` string: rcc source file to extract ts_files from; cannot be used
   with ts_files kwarg.
 - `rcc_extra_arguments` string[]: any additional arguments to `rcc` (optional),
   when used with `qresource.

Returns either: a list of custom targets for the compiled
translations, or, if using a `qresource` file, a single custom target
containing the processed source file, which should be passed to a main
build target.

## has_tools

This method returns `true` if all tools used by this module are found,
`false` otherwise.

It should be used to compile optional Qt code:
```meson
qt6 = import('qt6')
if qt6.has_tools(required: get_option('qt_feature'))
  moc_files = qt6.preprocess(...)
  ...
endif
```

This method takes the following keyword arguments:
- `required` bool | FeatureOption: by default, `required` is set to `false`. If `required` is set to
  `true` or an enabled [`feature`](Build-options.md#features) and some tools are
  missing Meson will abort.
- `method` string: The method to use to detect Qt, see [[dependency]]

## Dependencies

See [Qt dependencies](Dependencies.md#qt)

The 'modules' argument is used to include Qt modules in the project.
See the Qt documentation for the [list of
modules](https://doc.qt.io/qt-6/qtmodules.html).

The 'private_headers' argument allows usage of Qt's modules private
headers.

## Example
A simple example would look like this:

```meson
qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui'])
inc = include_directories('includes')
moc_files = qt6.compile_moc(headers : 'myclass.h',
                            extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
                            include_directories: inc,
                            dependencies: qt6_dep)
translations = qt6.compile_translations(ts_files : 'myTranslation_fr.ts', build_by_default : true)
executable('myprog', 'main.cpp', 'myclass.cpp', moc_files,
           include_directories: inc,
           dependencies : qt6_dep)
```

Sometimes, translations are embedded inside the binary using qresource
files. In this case the ts files do not need to be explicitly listed,
but will be inferred from the built qm files listed in the qresource
file. For example:

```meson
qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui'])
lang_cpp = qt6.compile_translations(qresource: 'lang.qrc')
executable('myprog', 'main.cpp', lang_cpp,
           dependencies: qt6_dep)
```

