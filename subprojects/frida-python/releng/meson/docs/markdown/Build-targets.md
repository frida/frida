---
short-description: Definition of build targets
...

# Build targets

Meson provides four kinds of build targets: executables, libraries
(which can be set to be built as static or shared or both of them at
the build configuration time), static libraries, and shared libraries.
They are created with the commands `executable`, `library`,
`static_library` and `shared_library`, respectively. All objects created
in this way are **immutable**. That is, you cannot change any aspect of
them after they have been constructed. This ensures that all information
pertaining to a given build target is specified in one well defined
place.

Libraries and executables
--

As an example, here is how you would build a library.

```meson
project('shared lib', 'c')
library('mylib', 'source.c')
```

It is generally preferred to use the `library` command instead of
`shared_library` and `static_library` and then configure which
libraries (static or shared or both of them) will be built at the
build configuration time using the `default_library`
[built-in option](Builtin-options.md).

In Unix-like operating systems, shared libraries can be
versioned. Meson supports this with keyword arguments, which will be
ignored if the library is configured as static at the compile time.

```meson
project('shared lib', 'c')
library('mylib', 'source.c', version : '1.2.3', soversion : '1')
```

It is common to build a library and then an executable that links
against it. This is supported as well.

```meson
project('shared lib', 'c')
lib = library('mylib', 'source.c')
executable('program', 'prog.c', link_with : lib)
```

Meson sets things up so that the resulting executable can be run
directly from the build directory. There is no need to write shell
scripts or set environment variables.

One target can have multiple language source files.

```meson
project('multilang', 'c', 'cpp')
executable('multiexe', 'file.c', 'file2.cc')
```

Object files
--

Sometimes you can't build files from sources but need to utilize an
existing object file. A typical case is using an object file provided
by a third party. Object files can be specified just like sources.

```meson
exe = executable('myexe', 'source.cpp', objects : 'third_party_object.o')
```

A different case is when you want to use object files built in one
target directly in another. A typical case is when you build a shared
library and it has an internal class that is not exported in the
ABI. This means you can't access it even if you link against the
library. Typical workarounds for this include building both a shared
and static version of the library or putting the source file in the
test executable's source list. Both of these approaches cause the
source to be built twice, which is slow.

In Meson you can extract object files from targets and use them as-is
on other targets. This is the syntax for it.

```meson
lib = shared_library('somelib', 'internalclass.cc', 'file.cc', ...)
eo = lib.extract_objects('internalclass.cc')
executable('classtest', 'classtest.cpp', objects : eo)
```

Here we take the internal class object and use it directly in the
test. The source file is only compiled once.

Note that careless use of this feature may cause strange bugs. As an
example trying to use objects of an executable or static library in a
shared library will not work because shared library objects require
special compiler flags. Getting this right is the user's
responsibility. For this reason it is strongly recommended that you
only use this feature for generating unit test executables in the
manner described above.
