# Command-line commands

There are two different ways of invoking Meson. First, you can run it
directly from the source tree with the command
`/path/to/source/meson.py`. Meson may also be installed in which case
the command is simply `meson`. In this manual we only use the latter
format for simplicity.

Meson is invoked using the following syntax:
`meson [COMMAND] [COMMAND_OPTIONS]`

This section describes all available commands and some of their
Optional arguments. The most common workflow is to run
[`setup`](#setup), followed by [`compile`](#compile), and then
[`install`](#install).

For the full list of all available options for a specific command use
the following syntax: `meson COMMAND --help`

### configure

{{ configure_usage.inc }}

Changes options of a configured meson project.

{{ configure_arguments.inc }}

Most arguments are the same as in [`setup`](#setup).

Note: reconfiguring project will not reset options to their default
values (even if they were changed in `meson.build`).

#### Examples:

List all available options:
```
meson configure builddir
```

Change value of a single option:
```
meson configure builddir -Doption=new_value
```

### compile

*(since 0.54.0)*

{{ compile_usage.inc }}

Builds a default or a specified target of a configured Meson project.

{{ compile_arguments.inc }}

`--verbose` argument is available since 0.55.0.

#### Targets

*(since 0.55.0)*

`TARGET` has the following syntax `[PATH/]NAME.SUFFIX[:TYPE]`, where:
- `NAME`: name of the target from `meson.build` (e.g. `foo` from `executable('foo', ...)`).
- `SUFFIX`: name of the suffix of the target from `meson.build` (e.g. `exe` from `executable('foo', suffix: 'exe', ...)`).
- `PATH`: path to the target relative to the root `meson.build` file. Note: relative path for a target specified in the root `meson.build` is `./`.
- `TYPE`: type of the target. Can be one of the following: 'executable', 'static_library', 'shared_library', 'shared_module', 'custom', 'alias', 'run', 'jar'.

`PATH`, `SUFFIX`, and `TYPE` can all be omitted if the resulting `TARGET` can be
used to uniquely identify the target in `meson.build`.

Note that `SUFFIX` did not exist prior to 1.3.0.

#### Backend specific arguments

*(since 0.55.0)*

`BACKEND-args` use the following syntax:

If you only pass a single string, then it is considered to have all
values separated by commas. Thus invoking the following command:

```
$ meson compile --ninja-args=-n,-d,explain
```

would add `-n`, `-d` and `explain` arguments to ninja invocation.

If you need to have commas or spaces in your string values, then you
need to pass the value with proper shell quoting like this:

```
$ meson compile "--ninja-args=['a,b', 'c d']"
```

#### Examples:

Build the project:
```
meson compile -C builddir
```

Execute a dry run on ninja backend with additional debug info:

```
meson compile --ninja-args=-n,-d,explain
```

Build three targets: two targets that have the same `foo` name, but
different type, and a `bar` target:

```
meson compile foo:shared_library foo:static_library bar
```

Produce a coverage html report (if available):

```
ninja coverage-html
```

### dist

*(since 0.52.0)*

{{ dist_usage.inc }}

Generates a release archive from the current source tree.

{{ dist_arguments.inc }}

See [notes about creating releases](Creating-releases.md) for more info.

#### Examples:

Create a release archive:
```
meson dist -C builddir
```

### init

*(since 0.45.0)*

{{ init_usage.inc }}

Creates a basic set of build files based on a template.

{{ init_arguments.inc }}

#### Examples:

Create a project in `sourcedir`:
```
meson init -C sourcedir
```

### env2mfile

*This command is experimental and subject to change.*

*{Since 0.62.0}*

{{ env2mfile_usage.inc }}

Create native and cross files from the current environment, typically
by sniffing environment variables like `CC` and `CFLAGS`.

{{ env2mfile_arguments.inc }}

#### Examples:

Autodetect the current cross build environment:

```
meson env2mfile --cross -o current_cross.txt --cpu=arm7a --cpu-family=arm --system=linux
```

Generate a cross build using Debian system information:

```
meson env2mfile --cross --debarch=armhf -o deb_arm_cross.txt
```



### introspect

{{ introspect_usage.inc }}

Displays information about a configured Meson project.

{{ introspect_arguments.inc }}

#### Examples:

Display basic information about a configured project in `builddir`:

```
meson introspect builddir --projectinfo
```

### install

*(since 0.47.0)*

{{ install_usage.inc }}

Installs the project to the prefix specified in [`setup`](#setup).

{{ install_arguments.inc }}

See [the installation documentation](Installing.md) for more info.

#### Examples:

Install project to `prefix`:
```
meson install -C builddir
```

Install project to `$DESTDIR/prefix`:
```
DESTDIR=/path/to/staging/area meson install -C builddir
```

Since *0.60.0* `DESTDIR` and `--destdir` can be a path relative to build
directory. An absolute path will be set into environment when executing scripts.

### rewrite

*(since 0.50.0)*

{{ rewrite_usage.inc }}

Modifies the Meson project.

{{ rewrite_arguments.inc }}

See [the Meson file rewriter documentation](Rewriter.md) for more info.

### setup

{{ setup_usage.inc }}

Configures a build directory for the Meson project.

*Deprecated since 0.64.0*: This is the default Meson command (invoked if there
was no COMMAND supplied). However, supplying the command is necessary to avoid
clashes with future added commands, so "setup" should be used explicitly.

*Since 1.1.0* `--reconfigure` is allowed even if the build directory does not
already exist, that argument is ignored in that case.

*Since 1.3.0* If the build directory already exists, options are updated with
their new value given on the command line (`-Dopt=value`). Unless `--reconfigure`
is also specified, this won't reconfigure immediately. This has the same behaviour
as `meson configure <builddir> -Dopt=value`.

*Since 1.3.0* It is possible to clear the cache and reconfigure in a single command
with `meson setup --clearcache --reconfigure <builddir>`.

{{ setup_arguments.inc }}

See [Meson introduction
page](Running-Meson.md#configuring-the-build-directory) for more info.

#### Examples:

Configures `builddir` with default values:
```
meson setup builddir
```

### subprojects

*(since 0.49.0)*

{{ subprojects_usage.inc }}

Manages subprojects of the Meson project. *Since 0.59.0* commands are run on
multiple subprojects in parallel by default, use `--num-processes=1` if it is
not desired.

Since *0.64.0* the `update` subcommand will not download new wrap files
from WrapDB any more. Use `meson wrap update` command for that instead.

{{ subprojects_arguments.inc }}

### test

{{ test_usage.inc }}

Run tests for the configure Meson project.

{{ test_arguments.inc }}

See [the unit test documentation](Unit-tests.md) for more info.

Since *1.2.0* you can use wildcards in *args* for test names.
For example, "bas*" will match all test with names beginning with "bas".

Since *1.2.0* it is an error to provide a test name or wildcard that
does not match any test.

#### Examples:

Run tests for the project:
```
meson test -C builddir
```

Run only `specific_test_1` and `specific_test_2`:
```
meson test -C builddir specific_test_1 specific_test_2
```

### wrap

{{ wrap_usage.inc }}

An utility to manage WrapDB dependencies.

{{ wrap_arguments.inc }}

See [the WrapDB tool documentation](Using-wraptool.md) for more info.

### devenv

*(since 0.58.0)*

{{ devenv_usage.inc }}

Runs a command, or open interactive shell if no command is provided, with
environment setup to run project from the build directory, without installation.

We automatically handle `bash` and set `$PS1` accordingly. If the automatic `$PS1`
override is not desired (maybe you have a fancy custom prompt), set the
`$MESON_DISABLE_PS1_OVERRIDE` environment variable and use `$MESON_PROJECT_NAME`
when setting the custom prompt, for example with a snippet like the following:

```bash
...
if [[ -n "${MESON_PROJECT_NAME-}" ]];
then
  PS1+="[ ${MESON_PROJECT_NAME} ]"
fi
...
```

These variables are set in environment in addition to those set using [[meson.add_devenv]]:
- `MESON_DEVENV` is defined to `'1'`.
- `MESON_PROJECT_NAME` is defined to the main project's name.
- `PKG_CONFIG_PATH` includes the directory where Meson generates `-uninstalled.pc`
  files.
- `PATH` includes every directory where there is an executable that would be
  installed into `bindir`. On windows it also includes every directory where there
  is a DLL needed to run those executables.
- `LD_LIBRARY_PATH` includes every directory where there is a shared library that
  would be installed into `libdir`. This allows to run system application using
  custom build of some libraries. For example running system GEdit when building
  GTK from git. On OSX the environment variable is `DYLD_LIBRARY_PATH` and
  `PATH` on Windows.
- `GI_TYPELIB_PATH` includes every directory where a GObject Introspection
  typelib is built. This is automatically set when using `gnome.generate_gir()`.
- `GSETTINGS_SCHEMA_DIR` *Since 0.59.0* includes every directory where a GSettings
  schemas is compiled. This is automatically set when using `gnome.compile_schemas()`.
  Note that this requires GLib >= 2.64 when `gnome.compile_schemas()` is used in
  more than one directory.
- `QEMU_LD_PREFIX` *Since 1.0.0* is set to the `sys_root` value from cross file
  when cross compiling and that property is defined.

*Since 0.62.0* if bash-completion scripts are being installed and the
shell is bash, they will be automatically sourced.

*Since 0.62.0* when GDB helper scripts (*-gdb.py, *-gdb.gdb, and *-gdb.csm)
are installed with a library name that matches one being built, Meson adds the
needed auto-load commands into `<builddir>/.gdbinit` file. When running gdb from
top build directory, that file is loaded by gdb automatically. In the case of
python scripts that needs to load other python modules, `PYTHONPATH` may need
to be modified using `meson.add_devenv()`.

*Since 0.63.0* when cross compiling for Windows `WINEPATH` is used instead
of `PATH` which allows running Windows executables using wine. Note that since
`WINEPATH` size is currently limited to 1024 characters, paths relative to the
root of build directory are used. That means current workdir must be the root of
build directory when running wine.

*Since 1.1.0* `meson devenv --dump [<filename>]` command takes an optional
filename argument to write the environment into a file instead of printing to
stdout.

*Since 1.1.0* `--dump-format` argument has been added to select which shell
format should be used. There are currently 3 formats supported:
- `sh`: Lines are in the format `VAR=/prepend:$VAR:/append`.
- `export`: Same as `sh` but with extra `export VAR` lines.
- `vscode`: Same as `sh` but without `$VAR` substitution because they do not
  seems to be properly supported by vscode.

{{ devenv_arguments.inc }}
