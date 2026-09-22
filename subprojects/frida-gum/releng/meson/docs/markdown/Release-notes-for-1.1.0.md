---
title: Release 1.1.0
short-description: Release notes for 1.1.0
...

# New features

Meson 1.1.0 was released on 10 April 2023
## `clang-cl` now accepts `cpp_std=c++20`

Requires `clang-cl` 13 or later.

## coercing values in the option() function is deprecated

Currently code such as:
```meson
option('foo', type : 'boolean', value : 'false')
```
works, because Meson coerces `'false'` to `false`.

This should be avoided, and will now result in a deprecation warning.

## New `declare_dependency(objects: )` argument

A new argument to `declare_dependency` makes it possible to add objects
directly to executables that use an internal dependency, without going
for example through `link_whole`.

## Dump devenv into file and select format

`meson devenv --dump [<filename>]` command now takes an optional filename argument
to write the environment into a file instead of printing to stdout.

A new `--dump-format` argument has been added to select which shell format
should be used. There are currently 3 formats supported:
- `sh`: Lines are in the format `VAR=/prepend:$VAR:/append`.
- `export`: Same as `sh` but with extra `export VAR` lines.
- `vscode`: Same as `sh` but without `$VAR` substitution because they do not
  seems to be properly supported by vscode.

## Feature objects now have an enable_auto_if method

This performs the opposite task of the disable_auto_if method, enabling the
feature if the condition is true.

## Add a FeatureOption.enable_if and .disable_if

These are useful when features need to be constrained to pass to [[dependency]],
as the behavior of an `auto` and `disabled` or `enabled` feature is markedly
different. consider the following case:

```meson
opt = get_option('feature').disable_auto_if(not foo)
if opt.enabled() and not foo
  error('Cannot enable feat when foo is not also enabled')
endif
dep = dependency('foo', required : opt)
```

This could be simplified to
```meson
opt = get_option('feature').disable_if(not foo, error_message : 'Cannot enable feature when foo is not also enabled')
dep = dependency('foo', required : opt)
```

For a real life example, here is some code in mesa:
```meson
_llvm = get_option('llvm')
dep_llvm = null_dep
with_llvm = false
if _llvm.allowed()
  dep_llvm = dependency(
    'llvm',
    version : _llvm_version,
    modules : llvm_modules,
    optional_modules : llvm_optional_modules,
    required : (
      with_amd_vk or with_gallium_radeonsi or with_gallium_opencl or with_clc
      or _llvm.enabled()
    ),
    static : not _shared_llvm,
    fallback : ['llvm', 'dep_llvm'],
    include_type : 'system',
  )
  with_llvm = dep_llvm.found()
endif
if with_llvm
  ...
elif with_amd_vk and with_aco_tests
  error('ACO tests require LLVM, but LLVM is disabled.')
elif with_gallium_radeonsi or with_swrast_vk
  error('The following drivers require LLVM: RadeonSI, SWR, Lavapipe. One of these is enabled, but LLVM is disabled.')
elif with_gallium_opencl
  error('The OpenCL "Clover" state tracker requires LLVM, but LLVM is disabled.')
elif with_clc
  error('The CLC compiler requires LLVM, but LLVM is disabled.')
else
  draw_with_llvm = false
endif
```

simplified to:
```meson
_llvm = get_option('llvm') \
  .enable_if(with_amd_vk and with_aco_tests, error_message : 'ACO tests requires LLVM') \
  .enable_if(with_gallium_radeonsi, error_message : 'RadeonSI requires LLVM') \
  .enable_if(with_swrast_vk, error_message : 'Vulkan SWRAST requires LLVM') \
  .enable_if(with_gallium_opencl, error_message : 'The OpenCL Clover state trackers requires LLVM') \
  .enable_if(with_clc, error_message : 'CLC library requires LLVM')

dep_llvm = dependency(
  'llvm',
  version : _llvm_version,
  modules : llvm_modules,
  optional_modules : llvm_optional_modules,
  required : _llvm,
  static : not _shared_llvm,
  fallback : ['llvm', 'dep_llvm'],
  include_type : 'system',
)
with_llvm = dep_llvm.found()
```

## Generated objects can be passed in the `objects:` keyword argument

In previous versions of Meson, generated objects could only be
passed as sources of a build target.  This was confusing, therefore
generated objects can now be passed in the `objects:` keyword
argument as well.

## The project function now supports setting the project license files

This goes together with the license name. The license files can be
automatically installed via [[meson.install_dependency_manifest]],
or queried via [[meson.project_license_files]].

## A new core directory option "licensedir" is available

This will install a dependency manifest to the specified directory, if none
is is explicitly set.

## `sudo meson install` now drops privileges when rebuilding targets

It is common to install projects using sudo, which should not affect build
outputs but simply install the results. Unfortunately, since the ninja backend
updates a state file when run, it's not safe to run ninja as root at all.

It has always been possible to carefully build with:

```
ninja && sudo meson install --no-rebuild
```

Meson now tries to be extra safe as a general solution. `sudo meson install`
will attempt to rebuild, but has learned to run `ninja` as the original
(pre-sudo or pre-doas) user, ensuring that build outputs are generated/compiled
as non-root.

## `meson install` now supports user-preferred root elevation tools

Previously, when installing a project, if any files could not be installed due
to insufficient permissions the install process was automatically re-run using
polkit. Now it prompts to ask whether that is desirable, and checks for
CLI-based tools such as sudo or opendoas or `$MESON_ROOT_CMD`, first.

Meson will no longer attempt privilege elevation at all, when not running
interactively.

## Support for reading options from meson.options

Support has been added for reading options from `meson.options` instead of
`meson_options.txt`. These are equivalent, but not using the `.txt` extension
for a build file has a few advantages, chief among them many tools and text
editors expect a file with the `.txt` extension to be plain text files, not
build scripts.

## Redirect introspection outputs to stderr

`meson introspect` used to disable logging to `stdout` to not interfere with generated json.
It now redirect outputs to `stderr` to allow printing warnings to the console
while keeping `stdout` clean for json outputs.

## New "none" backend

The `--backend=none` option has been added, to configure a project that has no
build rules, only install rules. This avoids depending on ninja.

## compiler.preprocess()

Dependencies keyword argument can now be passed to `compiler.preprocess()` to
add include directories or compiler arguments.

Generated sources such as custom targets are now allowed too.

## New pybind11 custom dependency

`dependency('pybind11')` works with pkg-config and cmake without any special
support, but did not handle the `pybind11-config` script.

This is useful because the config-tool will work out of the box when pybind11
is installed, but the pkg-config and cmake files are shoved into python's
site-packages, which makes it impossible to use in an out of the box manner.


## Allow --reconfigure and --wipe of empty builddir

`meson setup --reconfigure builddir` and `meson setup --wipe builddir` are now
accepting `builddir/` to be empty or containing a previously failed setup attempt.
Note that in that case previously passed command line options must be repeated
as only a successful build saves configured options.

This is useful for example with scripts that always repeat all options,
`meson setup builddir --wipe -Dfoo=bar` will always work regardless whether
it is a first invocation or not.

## Allow custom install scripts to run with `--dry-run` option

An new `dry_run` keyword is added to `meson.add_install_script()`
to allow a custom install script to run when meson is invoked
with `meson install --dry-run`. 

In dry run mode, the `MESON_INSTALL_DRY_RUN` environment variable
is set.

