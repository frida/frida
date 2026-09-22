## compile_resources

*New in 0.59.0*

Compiles Qt's resources collection files (.qrc) into c++ files for compilation.

It takes no positional arguments, and the following keyword arguments:
  - `name` (string | empty): if provided a single .cpp file will be generated,
    and the output of all qrc files will be combined in this file, otherwise
    each qrc file be written to its own cpp file.
  - `sources` (File | string | custom_target | custom_target index | generator_output)[]:
    A list of sources to be transpiled. Required, must have at least one source
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-rcc`
  - `method` string: The method to use to detect Qt, see `dependency()` for more
    information.

## compile_ui

*New in 0.59.0*

Compiles Qt's ui files (.ui) into header files.

It takes no positional arguments, and the following keyword arguments:
  - `sources` (File | string | custom_target | custom_target index | generator_output)[]:
    A list of sources to be transpiled. Required, must have at least one source
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-uic`
  - `method` string: The method to use to detect Qt, see `dependency()` for more
    information.
  - `preserve_paths` bool: *Since 1.4.0*. If `true`, specifies that the output
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
    A list of sources to be transpiled into .moc files for manual inclusion.
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `headers` (File | string | custom_target | custom_target index | generator_output)[]:
     A list of headers to be transpiled into .cpp files
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
  - `extra_args` string[]: Extra arguments to pass directly to `qt-moc`
  - `method` string: The method to use to detect Qt, see `dependency()` for more
    information.
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

If the `name` parameter is passed then all of the rcc files will be written to a single output file

The variadic `sources` arguments have been deprecated since Meson 0.59.0, as has the `sources` keyword argument. These passed files unmodified through the preprocessor, don't do this, just add the output of the generator to another sources list:
```meson
sources = files('a.cpp', 'main.cpp', 'bar.c')
sources += qt.preprocess(qresources : ['resources'])
```

This method takes the following keyword arguments:
 - `qresources` (string | File)[]: Passed to the RCC compiler
 - `ui_files`: (string | File | CustomTarget)[]: Passed the `uic` compiler
 - `moc_sources`: (string | File | CustomTarget)[]: Passed the `moc` compiler. These are converted into .moc files meant to be `#include`ed
 - `moc_headers`: (string | File | CustomTarget)[]: Passed the `moc` compiler. These will be converted into .cpp files
 - `include_directories` (IncludeDirectories | string)[], the directories to add to header search path for `moc`
 - `moc_extra_arguments` string[]: any additional arguments to `moc`. Since v0.44.0.
 - `uic_extra_arguments` string[]: any additional arguments to `uic`. Since v0.49.0.
 - `rcc_extra_arguments` string[]: any additional arguments to `rcc`. Since v0.49.0.
 - `dependencies` Dependency[]: dependency objects needed by moc. Available since v0.48.0.
 - `sources`: a list of extra sources, which are added to the output unchanged. Deprecated in 0.59.0.
 - `preserve_paths` bool: *New in 1.4.0*. If `true`, specifies that the output
   files need to maintain their directory structure inside the target temporary
   directory. For instance, when a file called `subdir/one.input` is processed
   it generates a file `{target private directory}/subdir/one.out` when `true`,
   and `{target private directory}/one.out` when `false` (default).

It returns an array of targets and sources to pass to a compilation target.

## compile_translations

*since 0.44.0*

This method generates the necessary targets to build translation files with
lrelease, it takes no positional arguments, and the following keyword arguments:

 - `ts_files` (File | string | custom_target | custom_target index | generator_output)[]:
    the list of input translation files produced by Qt's lupdate tool.
    *New in 0.60.0*: support for custom_target, custom_target_index, and generator_output.
 - `install` bool: when true, this target is installed during the install step (optional).
 - `install_dir` string: directory to install to (optional).
 - `build_by_default` bool: when set to true, to have this target be built by
   default, that is, when invoking `meson compile`; the default value is false
   (optional).
 - `qresource` string: rcc source file to extract ts_files from; cannot be used
   with ts_files kwarg. Available since v0.56.0.
 - `rcc_extra_arguments` string[]: any additional arguments to `rcc` (optional),
   when used with `qresource. Available since v0.56.0.

Returns either: a list of custom targets for the compiled
translations, or, if using a `qresource` file, a single custom target
containing the processed source file, which should be passed to a main
build target.

## has_tools

*since 0.54.0*

This method returns `true` if all tools used by this module are found,
`false` otherwise.

It should be used to compile optional Qt code:
```meson
qt5 = import('qt5')
if qt5.has_tools(required: get_option('qt_feature'))
  moc_files = qt5.preprocess(...)
  ...
endif
```

This method takes the following keyword arguments:
- `required` bool | FeatureOption: by default, `required` is set to `false`. If `required` is set to
  `true` or an enabled [`feature`](Build-options.md#features) and some tools are
  missing Meson will abort.
- `method` string: method used to find the Qt dependency (`auto` by default).

## Dependencies

See [Qt dependencies](Dependencies.md#qt)

The 'modules' argument is used to include Qt modules in the project.
See the Qt documentation for the [list of
modules](http://doc.qt.io/qt-5/qtmodules.html).

The 'private_headers' argument allows usage of Qt's modules private
headers. (since v0.47.0)

## Example
A simple example would look like this:

```meson
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])
inc = include_directories('includes')
moc_files = qt5.compile_moc(headers : 'myclass.h',
                            extra_args: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
                            include_directories: inc,
                            dependencies: qt5_dep)
translations = qt5.compile_translations(ts_files : 'myTranslation_fr.ts', build_by_default : true)
executable('myprog', 'main.cpp', 'myclass.cpp', moc_files,
           include_directories: inc,
           dependencies : qt5_dep)
```

Sometimes, translations are embedded inside the binary using qresource
files. In this case the ts files do not need to be explicitly listed,
but will be inferred from the built qm files listed in the qresource
file. For example:

```meson
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])
lang_cpp = qt5.compile_translations(qresource: 'lang.qrc')
executable('myprog', 'main.cpp', lang_cpp,
           dependencies: qt5_dep)
```
