---
short-description: Targets to run external commands
...

# Run targets

Sometimes you need to have a target that just runs an external
command. As an example you might have a build target that reformats
your source code, runs `cppcheck` or something similar. In Meson this
is accomplished with a so called *run target*.

The recommended way of doing this is writing the command(s) you want
to run to a script file. Here's an example script.

```bash
#!/bin/sh

cd "${MESON_SOURCE_ROOT}"
inspector_command -o "${MESON_BUILD_ROOT}/inspection_result.txt"
```

Note the two environment variables `MESON_SOURCE_ROOT` and
`MESON_BUILD_ROOT`. These are absolute paths to your project's source
and build directories and they are automatically set up by Meson. In
addition to these Meson also sets up the variable `MESON_SUBDIR`,
which points to the subdirectory where the run command was specified.
Most commands don't need to set up this.

Note how the script starts by cd'ing into the source dir. Meson does
not guarantee that the script is run in any specific directory.
Whether you need to do the same depends on what your custom target
wants to do.

To make this a run target we write it to a script file called
`scripts/inspect.sh` and specify it in the top level Meson file like
this.

```meson
run_target('inspector',
  command : 'scripts/inspect.sh')
```

Run targets are not run by default. To run it run the following command.

```console
$ meson compile inspector
```

All additional entries in `run_target`'s `command` array are passed
unchanged to the inspector script, so you can do things like this:

```meson
run_target('inspector',
  command : ['scripts/inspect.sh', '--exclude', 'tests'])
```
