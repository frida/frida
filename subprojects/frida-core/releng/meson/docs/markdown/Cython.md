---
title: Cython
short-description: Support for Cython in Meson
...

# Cython

Meson provides native support for cython programs starting with version 0.59.0.
This means that you can include it as a normal language, and create targets like
any other supported language:

```meson
lib = static_library(
    'foo',
    'foo.pyx',
)
```

Generally Cython is most useful when combined with the python module's
extension_module method:

```meson
project('my project', 'cython')

py = import('python').find_installation()
dep_py = py.dependency()

py.extension_module(
    'foo',
    'foo.pyx',
    dependencies : dep_py,
)
```

## C++ intermediate support

*(New in 0.60.0)*

An option has been added to control this, called `cython_language`. This can be
either `'c'` or `'cpp'`.

For those coming from setuptools/distutils, they will find two things. First,
meson ignores `# distutils: language = c++` inline directives. Second that Meson
allows options only on a per-target granularity. This means that if you need to mix
cython files being transpiled to C and to C++ you need two targets:

```meson
project('my project', 'cython')

cython_cpp_lib = static_library(
    'helper_lib',
    'foo_cpp.pyx',  # will be transpiled to C++
    override_options : ['cython_language=cpp'],
)

py.extension_module(
    'foo',
    'foo.pyx',  # will be transpiled to C
    link_with : [cython_cpp_lib],
    dependencies : dep_py,
)
```
