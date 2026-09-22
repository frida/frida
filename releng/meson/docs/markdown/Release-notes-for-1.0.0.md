---
title: Release 1.0.0
short-description: Release notes for 1.0.0
...

# New features

Meson 1.0.0 was released on 23 December 2022
## Compiler check functions `prefix` kwargs accepts arrays

The `prefix` kwarg that most compiler check functions support
now accepts an array in addition to a string. The elements of the
array will be concatenated separated by a newline.

This makes it more readable to write checks that need multiple headers
to be included:

```meson
cc.check_header('GL/wglew.h', prefix : ['#include <windows.h>', '#include <GL/glew.h>'])
```

instead of

```meson
cc.check_header('GL/wglew.h', prefix : '#include <windows.h>\n#include <GL/glew.h>'])
```

## Flags removed from cpp/objcpp warning level 1

`-Wnon-virtual-dtor` is no longer implied by `meson setup -Dwarning_level=1`.

## Developer environment improvements

When cross compiling, the developer environment now sets all environment
variables for the HOST machine. It now also sets `QEMU_LD_PREFIX` to the
`sys_root` value from cross file if property is defined. That means that cross
compiled executables can often be run transparently on the build machine, for
example when cross compiling for aarch64 linux from x86_64 linux.

A new argument `--workdir` has been added, by default it is set to build
directory. For example, `meson devenv -C builddir --workdir .` can be used to
remain in the current dir (often source dir) instead.

`--dump` now prints shell commands like `FOO="/prepend/path:$FOO:/append/path"`,
using the literal `$FOO` instead of current value of `FOO` from environment.
This makes easier to evaluate those expressions in a different environment.

## Deprecate `java.generate_native_headers`, rename to `java.native_headers`

The functions operate in the exact same way. The new name matches more with
Meson function name styling.

## rust.bindgen accepts a dependency argument

The `bindgen` method of the `rust` module now accepts a dependencies argument.
Any include paths in these dependencies will be passed to the underlying call to
`clang`, and the call to `bindgen` will correctly depend on any generated sources.

## String arguments to the rust.bindgen include_directories argument

Most other cases of include_directories accept strings as well as
`IncludeDirectory` objects, so lets do that here too for consistency.

## The Rust module is stable

Mesa is using the rust module in production, so it's time to mark it as stable.

## `in` operator for strings

`in` and `not in` operators now works on strings, in addition to arrays and
dictionaries.

```
fs = import('fs')
if 'something' in fs.read('somefile')
  # True
endif
```

## `warning-level=everything` option

The new `everything` value for the built-in `warning_level` enables roughly all applicable compiler warnings.
For clang and MSVC, this simply enables `-Weverything` or `/Wall`, respectively.
For GCC, meson enables warnings approximately equivalent to `-Weverything` from clang.

