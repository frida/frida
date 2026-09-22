# Shipping prebuilt binaries as wraps

A common dependency case, especially on Windows, is the need to
provide dependencies as prebuilt binaries rather than Meson projects
that you build from scratch. Common reasons include not having access
to source code, not having the time and effort to rewrite a legacy
system's build definitions to Meson or just the fact that compiling
the dependency projects takes too long.

Packaging a project is straightforward. As an example let's look at a
case where the project consists of one static library called `bob` and
some headers. To create a binary dependency project we put the static
library at the top level and headers in a subdirectory called
`include`. The Meson build definition would look like the following.

```meson
project('bob', 'c')

# Do some sanity checking so that meson can fail early instead of at final link time
if not (host_machine.system() == 'windows' and host_machine.cpu_family() == 'x86_64')
  error('This wrap of libbob is a binary wrap for x64_64 Windows, and will not work on your system')
endif

cc = meson.get_compiler('c')
bob_dep = declare_dependency(
  dependencies : cc.find_library('bob', dirs : meson.current_source_dir()),
  include_directories : include_directories('include'))

meson.override_dependency('bob', bob_dep)
```

Now you can use this subproject as if it was a Meson project:

```meson
project('using dep', 'c')
bob_dep = dependency('bob')
executable('prog', 'prog.c', dependencies : bob_dep)
```

Note that often libraries compiled with different compilers (or even
compiler flags) might not be compatible. If you do this, then you are
responsible for verifying that your libraries are compatible, Meson
will not check things for you.

## Using a wrap file

To make this all work automatically, a project will need a
[wrap file](Wrap-dependency-system-manual.md#wrap-format), as well as the
meson.build definition from above. For this example our dependency is called
`bob`.

The wrap ini (subprojects/bob.wrap):
```ini
[wrap-file]
directory = libbob-1.0
source_url = https://libbob.example.com/libbob-1.0.zip
source_filename = libbob-1.0.zip
source_hash = 5ebeea0dfb75d090ea0e7ff84799b2a7a1550db3fe61eb5f6f61c2e971e57663
patch_directory = libbob

[provide]
dependency_names = bob
```

Then create `subprojects/packagefiles/libbob/`, and place the `meson.build` from
above in that directory. With these in place a call to `dependency('bob')` will
first try standard discovery methods for your system (such as pkg-config, cmake,
and any built-in meson find methods), and then fall back to using the binary
wrap if it cannot find the dependency on the system. Meson provides the
`--force-fallback-for=bob` command line option to force the use of the fallback.

## Note for Linux libraries

A precompiled linux shared library (.so) requires a soname field to be properly installed. If the soname field is missing, binaries referencing the library will require a hard link to the location of the library at install time (`/path/to/your/project/subprojects/precompiledlibrary/lib.so` instead of `$INSTALL_PREFIX/lib/lib.so`) after installation.

You should change the compilation options for the precompiled library to avoid this issue. If recompiling is not an option, you can use the [patchelf](https://github.com/NixOS/patchelf) tool with the command `patchelf --set-soname libfoo.so libfoo.so` to edit the precompiled library after the fact.

Meson generally guarantees any library it compiles has a soname. One notable exception is libraries built with the [[shared_module]] function.
