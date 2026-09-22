# Python 3 module

This module provides support for dealing with Python 3. It has the
following methods.

This module is deprecated and replaced by the
[python](Python-module.md) module.

## find_python

This is a cross platform way of finding the Python 3 executable, which
may have a different name on different operating systems. Returns an
[[@external_program]]
object.

*Added 0.38.0*

Deprecated, replaced by
[`find_installation`](Python-module.md#find_installation) function
from `python` module.

## extension_module

Creates a `shared_module` target that is named according to the naming
conventions of the target platform. All positional and keyword
arguments are the same as for
[[shared_module]].

`extension_module` does not add any dependencies to the library so user may
need to add `dependencies : dependency('python3')`, see
[Python3 dependency](Dependencies.md#python3).

*Added 0.38.0*

Deprecated, replaced by
[`extension_module`](Python-module.md#extension_module) method from
`python` module.

## language_version

Returns a string with the Python language version such as `3.5`.

*Added 0.40.0*

Deprecated, replaced by
[`language_version`](Python-module.md#language_version) method from
`python` module.

## sysconfig_path

Returns the Python sysconfig path without prefix, such as
`lib/python3.6/site-packages`.

*Added 0.40.0*

Deprecated, replaced by [`get_path`](Python-module.md#get_path)
method from `python` module.
