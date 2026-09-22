---
short-description: Style recommendations for Meson files
...

# Style recommendations

This page lists some recommendations on organizing and formatting your
Meson build files.

## Tabs or spaces?

Always spaces.

## Naming Variable

Snake case (stylized as `snake_case`) refers to the style of writing in which
each space is replaced by an underscore (`_`) character, and the first letter of
each word written in lowercase. It is the most common naming convention used
in Meson build scripts as identifiers for variable.

Let say you would like to refer to your executable so something like `my_exe`.

## Dependency usage

The `dependency` function is the recommended way to handle
dependencies. If your wrap files have the necessary `[provide]`
entries, everything will work automatically both when compiling your
own and when using system dependencies.

You should only need `subproject` when you need to extract non dependencies/programs.

## Naming options

There are two ways of naming project options. As an example for
booleans the first one is `foo` and the second one is `enable-foo`.
The former style is recommended, because in Meson options have strong
type, rather than being just strings.

You should try to name options the same as is common in other
projects. This is especially important for yielding options, because
they require that both the parent and subproject options have the same
name.

# Global arguments

Prefer `add_project_arguments` to `add_global_arguments` because using
the latter prevents using the project as a subproject.

# Cross compilation arguments

Try to keep cross compilation arguments away from your build files as
much as possible. Keep them in the cross file instead. This adds
portability, since all changes needed to compile to a different
platform are isolated in one place.

# Sorting source paths

The source file arrays should all be sorted. This makes it easier to
spot errors and often reduces merge conflicts. Furthermore, the paths
should be sorted with a natural sorting algorithm, so that numbers are
sorted in an intuitive way (`1, 2, 3, 10, 20` instead of `1, 10, 2,
20, 3`).

Numbers should also be sorted before characters (`a111` before `ab0`).
Furthermore, strings should be sorted case insensitive.

Additionally, if a path contains a directory it should be sorted before
normal files. This rule also applies recursively for subdirectories.

The following example shows correct source list definition:

```meson
sources = files([
  'aaa/a1.c',
  'aaa/a2.c',
  'bbb/subdir1/b1.c',
  'bbb/subdir2/b2.c',
  'bbb/subdir10/b3.c',
  'bbb/subdir20/b4.c',
  'bbb/b5.c',
  'bbb/b6.c',
  'f1.c',
  'f2.c',
  'f10.c',
  'f20.c'
])
```
