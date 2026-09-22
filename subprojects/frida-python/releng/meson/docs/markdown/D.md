---
title: D
short-description: Compiling D sources
...

# Compiling D applications

Meson has support for compiling D programs. A minimal `meson.build`
file for D looks like this:

```meson
project('myapp', 'd')

executable('myapp', 'app.d')
```

## [Conditional compilation](https://dlang.org/spec/version.html)

If you are using the
[version()](https://dlang.org/spec/version.html#version-specification)
feature for conditional compilation, you can use it using the
`d_module_versions` target property:

```meson
project('myapp', 'd')
executable('myapp', 'app.d', d_module_versions: ['Demo', 'FeatureA'])
```

For debugging, [debug()](https://dlang.org/spec/version.html#debug)
conditions are compiled automatically in debug builds, and extra
identifiers can be added with the `d_debug` argument:

```meson
project('myapp', 'd')
executable('myapp', 'app.d', d_debug: [3, 'DebugFeatureA'])
```

## In `declare_dependency`

*Since 0.62.0*, when declaring your own dependency using `declare_dependency`,
it is possible to add parameters for D specific features, e.g. to propagate
conditional compilation versions:

```meson
my_dep = declare_dependency(
    # ...
    d_module_versions: ['LUA_53'],
    d_import_dirs: include_directories('my_lua_folder'),
)
```

Accepted D specific parameters are `d_module_versions` and `d_import_dirs`
(DMD `-J` switch).

## Using embedded unittests

If you are using embedded [unittest
functions](https://dlang.org/spec/unittest.html), your source code
needs to be compiled twice, once in regular mode, and once with
unittests active. This is done by setting the `d_unittest` target
property to `true`. Meson will only ever pass the respective
compiler's `-unittest` flag, and never have the compiler generate an
empty main function. If you need that feature in a portable way,
create an empty `main()` function for unittests yourself, since the
GNU D compiler does not have this feature.

This is an example for using D unittests with Meson:
```meson
project('myapp_tested', 'd')

myapp_src = ['app.d', 'alpha.d', 'beta.d']
executable('myapp', myapp_src)

test_exe = executable('myapp_test', myapp_src, d_unittest: true)
test('myapptest', test_exe)
```

# Compiling D libraries and installing them

Building D libraries is a straightforward process, not different from
how C libraries are built in Meson. You should generate a pkg-config
file and install it, in order to make other software on the system
find the dependency once it is installed.

This is an example on how to build a D shared library:
```meson
project('mylib', 'd', version: '1.2.0')

project_soversion = 0
glib_dep = dependency('glib-2.0')

my_lib = library('mylib',
    ['src/mylib/libfunctions.d'],
    dependencies: [glib_dep],
    install: true,
    version: meson.project_version(),
    soversion: project_soversion,
    d_module_versions: ['FeatureA', 'featureB', 1]
)

pkgc = import('pkgconfig')
pkgc.generate(name: 'mylib',
              libraries: my_lib,
              subdirs: 'd/mylib',
              version: meson.project_version(),
              description: 'A simple example D library.',
              d_module_versions: ['FeatureA', 1]
)
install_subdir('src/mylib/', install_dir: 'include/d/mylib/')
```

It is important to make the D sources install in a subdirectory in the
include path, in this case `/usr/include/d/mylib/mylib`. All D
compilers include the `/usr/include/d` directory by default, and if
your library would be installed into `/usr/include/d/mylib`, there is
a high chance that, when you compile your project again on a machine
where you installed it, the compiler will prefer the old installed
include over the new version in the source tree, leading to very
confusing errors.

This is an example of how to use the D library we just built and
installed in an application:
```meson
project('myapp', 'd')

mylib_dep = dependency('mylib', version: '>= 1.2.0')
myapp_src = ['app.d', 'alpha.d', 'beta.d']
executable('myapp', myapp_src, dependencies: [mylib_dep])
```

Please keep in mind that the library and executable would both need to
be built with the exact same D compiler and D compiler version. The D
ABI is not stable across compilers and their versions, and mixing
compilers will lead to problems.

# Integrating with DUB

DUB is a fully integrated build system for D, but it is also a way to
provide dependencies. Adding dependencies from the [D package
registry](https://code.dlang.org/) is pretty straight forward. You can
find how to do this in
[Dependencies](Dependencies.md#some-notes-on-dub). You can also
automatically generate a `dub.json` file as explained in
[Dlang](Dlang-module.md#generate_dub_file).
