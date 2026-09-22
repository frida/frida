---
short-description: Adding compiler arguments
...

# Adding arguments

Often you need to specify extra compiler arguments. Meson provides two
different ways to achieve this: global arguments and per-target
arguments.

Global arguments
--

Global compiler arguments are set with the following command. As an
example you could do this.

```meson
add_global_arguments('-DFOO=bar', language : 'c')
```

This makes Meson add the define to all C compilations. Usually you
would use this setting for flags for global settings. Note that for
setting the C/C++ language standard (the `-std=c99` argument in GCC),
you would probably want to use a default option of the [[project]]
function. For details see the [reference manual](Reference-manual.md).

Global arguments have certain limitations. They all have to be defined
before any build targets are specified. This ensures that the global
flags are the same for every single source file built in the entire
project with one exception. Compilation tests that are run as part of
your project configuration do not use these flags. The reason for that
is that you may need to run a test compile with and without a given
flag to determine your build setup. For this reason tests do not use
these global arguments.

You should set only the most essential flags with this setting, you
should *not* set debug or optimization flags. Instead they should be
specified by selecting an appropriate build type.

Project arguments
--

Project arguments work similar to global arguments except that they
are valid only within the current subproject. The usage is simple:

```meson
add_project_arguments('-DMYPROJ=projname', language : 'c')
```

This would add the compiler flags to all C sources in the current
project.

Per target arguments
--

Per target arguments are just as simple to define.

```meson
executable('prog', 'prog.cc', cpp_args : '-DCPPTHING')
```

Here we create a C++ executable with an extra argument that is used
during compilation but not for linking.

You can find the parameter name for other languages in the [reference
tables](Reference-tables.md).

Specifying extra linker arguments is done in the same way:

```meson
executable('prog', 'prog.cc', link_args : '-Wl,--linker-option')
```
