---
title: Release 0.61.0
short-description: Release notes for 0.61.0
...

# New features

## backend_startup_project

`backend_startup_project` will no longer erase the last project in a VS
solution if it is not the specified project.

## Windows.compile_resources CustomTarget

Previously the Windows module only accepted CustomTargets with one output, it
now accepts them with more than one output, and creates a windows resource
target for each output. Additionally it now accepts indexes of CustomTargets

```meson

ct = custom_target(
  'multiple',
  output : ['resource', 'another resource'],
  ...
)

ct2 = custom_target(
  'slice',
  output : ['resource', 'not a resource'],
  ...
)

resources = windows.compile_resources(ct, ct2[0])
```

## Add a man page backend to refman

The refman docs (function and object reference) can now be generated as a man
page.

## ``extract_objects()`` supports generated sources

Custom targets or generated files (returned by ``generator.process()``)
can now be passed to ``extract_objects()``.

## Python 3.6 support will be dropped in the next release

The final [Python 3.6 release was 3.6.15 in September](https://www.python.org/dev/peps/pep-0494/#lifespan).
This release series is now End-of-Life (EOL). The only LTS distribution that
still ships Python 3.5 as the default Python is Ubuntu 18.04, which has Python
3.8 available as well.

Python 3.7 has various features that we find useful such as future annotations,
the importlib.resources module, and dataclasses.

As a result, we will begin requiring Python 3.7 or newer in Meson 0.62, which
is the next release. Starting with Meson 0.61, we now print a `NOTICE:` when
a `meson` command is run on Python 3.6 to inform users about this.

## Warning if check kwarg of run_command is missing

The `check` kwarg of `run_command` currently defaults to `false`.
Because we want to change that, running
```meson
run_command('cmd')
```
now results in:
```text
WARNING: You should add the boolean check kwarg to the run_command call.
         It currently defaults to false,
         but it will default to true in future releases of meson.
         See also: https://github.com/mesonbuild/meson/issues/9300
```

## `meson rewrite` can modify `extra_files`

The build script rewriter can now modify targets' `extra_files` lists,
or create them if absent. It it used in the same way as with rewriting
source lists:

```bash
meson rewrite target <target name/id> {add_extra_files/rm_extra_files} [list of extra files]
```

The rewriter's script mode also supports these actions:

```json
{
  "type": "target",
  "target": "<target name>",
  "operation": "extra_files_add / extra_files_rm",
  "sources": ["list", "of", "extra", "files", "to", "add, remove"],
}
```

## `meson rewrite target <target> info` outputs *target*'s `extra_files`

Targets' `extra_files` lists are now included in the rewriter's target info dump
as a list of file paths, in the same way `sources` are. This applies to both
`meson rewrite` CLI and script mode.

## Visual Studio 2022 backend

As Visual Studio 2022 is released recently, it's time to support the
new version in Meson. This mainly includes the new "v143" platform tools.

The usage is similar to other backends. For example
```meson
meson setup builddir --backend=vs2022
```
will configure "builddir" for projects compatible with Visual Studio 2022.

## Support for CMake <3.14 is now deprecated for CMake subprojects

In CMake 3.14, the File API was introduced and the old CMake server API was
deprecated (and removed in CMake 3.20). Thus support for this API will also
be removed from Meson in future releases.

This deprecation only affects CMake subprojects.

## Added support for sccache

Meson now supports [sccache](https://github.com/mozilla/sccache) just
like it has supported CCache. If both sccache and CCache are
available, the autodetection logic prefers sccache.

## install_symlink function

It is now possible to request for symbolic links to be installed during
installation. The `install_symlink` function takes a positional argument to
the link name, and installs a symbolic link pointing to `pointing_to` target.
The link will be created under `install_dir` directory and cannot contain path
separators.

```meson
install_symlink('target', pointing_to: '../bin/target', install_dir: '/usr/sbin')
```

