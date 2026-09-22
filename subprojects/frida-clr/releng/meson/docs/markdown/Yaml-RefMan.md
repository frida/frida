---
title: YAML Reference manual
short-description: Editing and maintaining the Reference manual
...

# Reference Manual

The [Reference Manual](Reference-manual.md) is automatically generated out of YAML
files in `docs/yaml`. This allows the Meson project to enforce a consistent
style of the Reference Manual and enables easier style changes to the generated
Markdown files without touching the actual documentation.
Additionally, multiple generation backends can be supported (besides the
Markdown generator for mesonbuild.com).

The generator that reads these YAML files is located in `docs/refman`, with the
main executable being `docs/genrefman.py`.  By default `genrefman.py` will load
the yaml manual using a strict subset of yaml at the cost of loading slowly.
You may optionally disable all these safety checks using the `fastyaml` loader,
which will significantly speed things up at the cost of being less correct.

The following python packages are required for the `genrefman` script:

- chevron
- strictyaml

## Linking to the Reference Manual

Links to the Reference Manual can be inserted *anywhere* in the Meson docs with
tags like this: `[[<tag>]]`. This guarantees that links remain stable (even if
the structure of the Reference Manual changes) and are uniformly formatted
everywhere.

To link to functions, the function name should be put into the tag:
`[[<func name>]]`.
Methods (for all kinds of objects, including modules) can be linked to like
this: `[[<object name>.<method name>]]`.
To link to objects themselves, the `[[@<object name>]]` syntax can be used.

These tags do **not** need to be put in inline code! A hotdoc extension handles
the formatting here. If tags need to be placed (for instance, to include reference
directly in code blocks), the `[[#<remaining tag>]]` syntax should be used.

Examples:
- Functions: [[executable]]
- Methods: [[meson.version]]
- Objects: [[@str]]

Now the same in a code block:

```meson
[[#@str]] [[#executable]]('main', [
    'file_@0@.cpp'.format([[#meson.version]])
])
```


## Directory structure

The directory structure under `docs/yaml` is important, since it determines how
the YAML files are interpreted:

- `builtins`: Documentation for builtin objects, such as `meson`
- `elementary`: Strings, lists, integers, void, etc.
- `objects`: All Objects returned by functions and methods but **not** modules
- `functions`: All root meson functions ([[executable]], [[project]], etc.)

Finally, modules are defined inside the `modules` subdirectory. Here, each
module has its own directory. The module itself **must** be in a file called
`module.yaml`. All objects returned by the module are then located next to this
file.

The name of the YAML files themselves are ignored (with the exception of
`module.yaml`) and carry no specific meaning. However, it is recommended to name
the YAML files after the `name` entry of the object.

All objects and functions whose name starts with a `_` are marked as private and
are *not* exported in the final documents. The primary purpose of these files
is to make inheriting functions and arguments easier.



# YAML schema

The YAML files themselves are structured as follows:

## Functions

```yaml
name: executable     # The name of the function                [required]
returns: build_tgt   # Must be a `name` of an existing object  [required]
description: |
  The first line until the first dot of the description is the brief.
  All other lines are not part of the brief and should document the function

  Here the full Markdown syntax is supported, such as links, `inline code`,
  code blocks, and references to other parts of the Reference Manual: [[@str]].

  This is true for **all** description keys in all YAML files. Defining a
  description is **always** required.

since:      0.42.0       # A valid Meson version
deprecated: 100.99.0     # A valid Meson version

example: |
  Similar to `description`, but is put under a different section and should
  contain an example.

notes:
- A list of notes that should stand out.
- Should be used sparingly.
- Notes are optional.

warnings:
- Similar to notes, but a warning
- Warnings are also optional.


# To avoid duplicating documentation / code, argument inheritance is supported with
# the following optional keys:

posargs_inherit: _build_target_base  # Use the posargs definition of `_build_target_base` here
optargs_inherit: _build_target_base  # Use the optargs definition of `_build_target_base` here
varargs_inherit: _build_target_base  # Use the varargs definition of `_build_target_base` here
kwargs_inherit: _build_target_base   # Add all kwargs of `_build_target_base` to this function

# Whether argument flattening (see Syntax.md) is enabled
# for this function. Defaults to `true`.
args_flattening: true

posargs:
  arg_name:
    type: bool | dep           # [required]
    description: Some text.    # [required]
    since: 0.42.0
    deprecated: 100.99.0
    default: false             # Is technically supported buy should **not** be used for posargs

  another_arg:
    ...

optargs:
  optional_arg:
    type: int                  # [required]
    description: Hello World   # [required]
    since: 0.42.0
    deprecated: 100.99.0
    default: false             # Default values can make sense here

  next_arg:
    ...

varargs:
  name: Some name                # [required]
  type: str | list[str | int]    # [required]
  description: Some helpful text # [required]
  since: 0.42.0
  deprecated: 100.99.0
  min_varargs: 1
  max_varargs: 21


kwargs:
  kwarg_name:
    type: str                      # [required]
    description: Meson is great!   # [required]
    since: 0.42.0
    deprecated: 100.99.0
    default: false
    required: false                # Some kwargs may be required
```


## Objects

```yaml
name: build_tgt                       # [required]
long_name: Build target               # [required]
description: Just some description.   # [required]
example: Same as for functions

# Objects can be marked as containers. In this case they can be used in a `type`
# like this `container[held | objects]`. Currently this only makes sense for
# lists and dicts. There is almost no reason to set this to true for other objects.
is_container: true

since:      0.42.0
deprecated: 100.99.0

# Notes and warnings work the same as with functions
notes:
warnings:

# Inheritance is also supported for objects. Here all methods from the parent
# object are inherited. The trick with `_private` objects also works here
# to help with more complex structures.
extends: tgt

# Methods are a list of functions (see the previous section).
methods:
- ...
```
