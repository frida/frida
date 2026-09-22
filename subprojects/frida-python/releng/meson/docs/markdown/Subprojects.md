---
short-description: Using Meson projects as subprojects within other Meson projects
...

# Subprojects

Some platforms do not provide a native packaging system. In these
cases it is common to bundle all third party libraries in your source
tree. This is usually frowned upon because it makes it hard to add
these kinds of projects into e.g. those Linux distributions that
forbid bundled libraries.

Meson tries to solve this problem by making it extremely easy to
provide both at the same time. The way this is done is that Meson
allows you to take any other Meson project and make it a part of your
build without (in the best case) any changes to its Meson setup. It
becomes a transparent part of the project.

It should be noted that this is only guaranteed to work for subprojects
that are built with Meson. The reason is the simple fact that there is
no possible way to do this reliably with mixed build systems. Because of
this, only Meson subprojects are described here.
[CMake based subprojects](CMake-module.md#cmake-subprojects) are also
supported but not guaranteed to work.

## A subproject example

Usually dependencies consist of some header files plus a library to
link against. To declare this internal dependency use
`declare_dependency` function.

As an example, suppose we have a simple project that provides a shared
library. Its `meson.build` would look like this.

```meson
project('libsimple', 'c')

inc = include_directories('include')
libsimple = shared_library('simple',
  'simple.c',
  include_directories : inc,
  install : true)

libsimple_dep = declare_dependency(include_directories : inc,
  link_with : libsimple)
```

### Naming convention for dependency variables

Ideally the dependency variable name should be of `<project_name>_dep`
form. This way one can just use it without even looking inside build
definitions of that subproject.

In cases where there are multiple dependencies need to be declared,
the default one should be named as `<project_name>_dep` (e.g.
`gtest_dep`), and others can have `<project_name>_<other>_<name>_dep`
form (e.g. `gtest_main_dep` - gtest with main function).

There may be exceptions to these rules where common sense should be applied.

### Adding variables to the dependency

*New in 0.54.0*

In some cases a project may define special variables via pkg-config or
cmake that a caller needs to know about. Meson provides a
`dependency.get_variable` method to hide what kind of dependency is
provided, and this is available to subprojects as well. Use the
`variables` keyword to add a dict of strings:

```meson
my_dep = declare_dependency(..., variables : {'var': 'value', 'number': '3'})
```

Which another project can access via:

```meson
var = my_dep.get_variable(internal : 'var', cmake : 'CMAKE_VAR')
```

The values of the dict must be strings, as pkg-config and cmake will
return variables as strings.

### Build options in subproject

All Meson features of the subproject, such as project options keep
working and [can be set in the master
project](Builtin-options.md#specifying-options-per-subproject). There
are a few limitations, the most important being that global compiler
arguments must be set in the main project before calling subproject.
Subprojects must not set global arguments because there is no way to
do that reliably over multiple subprojects. To check whether you are
running as a subproject, use [[meson.is_subproject]].

## Using a subproject

All subprojects must be inside `subprojects` directory. The
`subprojects` directory must be at the top level of your project.
Subproject declaration must be in your top level `meson.build`.

### A simple example

Let's use `libsimple` as a subproject.

At the top level of your project create `subprojects` directory.
Then copy `libsimple` into `subprojects` directory.

Your project's `meson.build` should look like this.

```meson
project('my_project', 'cpp')

libsimple_proj = subproject('libsimple')
libsimple_dep = libsimple_proj.get_variable('libsimple_dep')

executable('my_project',
  'my_project.cpp',
  dependencies : libsimple_dep,
  install : true)
```

Note that the subproject object is *not* used as the dependency, but
rather you need to get the declared dependency from it with
`get_variable` because a subproject may have multiple declared
dependencies.

### Toggling between system libraries and embedded sources

When building distro packages it is very important that you do not
embed any sources. Some distros have a rule forbidding embedded
dependencies so your project must be buildable without them or
otherwise the packager will hate you.

Here's how you would use system libraries and fall back to embedding sources
if the dependency is not available.

```meson
project('my_project', 'cpp')

libsimple_dep = dependency('libsimple', required : false)

if not libsimple_dep.found()
  libsimple_proj = subproject('libsimple')
  libsimple_dep = libsimple_proj.get_variable('libsimple_dep')
endif

executable('my_project',
  'my_project.cpp',
  dependencies : libsimple_dep,
  install : true)
```

Because this is such a common operation, Meson provides a shortcut for
this use case.

```meson
dep = dependency('foo', fallback : ['subproject_name', 'variable_name'])
```

The `fallback` keyword argument takes two items, the name of the
subproject and the name of the variable that holds the dependency. If
you need to do something more complicated, such as extract several
different variables, then you need to do it yourself with the manual
method described above.

Using this shortcut the build definition would look like this.

```meson
project('my_project', 'cpp')

libsimple_dep = dependency('libsimple', fallback : ['libsimple', 'libsimple_dep'])

executable('my_project',
  'my_project.cpp',
  dependencies : libsimple_dep,
  install : true)
```

You may change default [options of the
subproject](Builtin-options.md#specifying-options-per-subproject) by
adding a keyword argument to the invocation. For example, to change the
default library type:

```
libsimple_dep = dependency(
  'libsimple',
  fallback : ['libsimple', 'libsimple_dep'],
  default_options: ['default_library=static']
)
```

With this setup, when libsimple is provided by the system we use it and
completely ignore subproject options (i.e. we link to a shared system
library). When that is not the case, we use the embedded version (the
one from subprojects).

Note that `libsimple_dep` can point to an external or an internal
dependency but you don't have to worry about their differences. Meson
will take care of the details for you.

### Subprojects depending on other subprojects

Subprojects can use other subprojects, but all subprojects must reside
in the top level `subprojects` directory. Recursive use of subprojects
is not allowed, though, so you can't have subproject `a` that uses
subproject `b` and have `b` also use `a`.

## Obtaining subprojects

Meson ships with a dependency system to automatically obtain
dependency subprojects. It is documented in the [Wrap dependency
system manual](Wrap-dependency-system-manual.md).

## Command-line options

The usage of subprojects can be controlled by users and distros with
the following command-line options:

* **--wrap-mode=nodownload**

    Meson will not use the network to download any subprojects or
    fetch any wrap information. Only preexisting sources will be used.
    This is useful (mostly for distros) when you want to only use the
    sources provided by a software release, and want to manually handle
    or provide missing dependencies.

* **--wrap-mode=nofallback**

    Meson will not use subproject fallbacks for any dependency
    declarations in the build files, and will only look for them in the
    system. Note that this does not apply to unconditional subproject()
    calls, and those are meant to be used for sources that cannot be
    provided by the system, such as copylibs.

    This option may be overridden by `--force-fallback-for` for specific
    dependencies.

* **--wrap-mode=forcefallback**

    Meson will not look at the system for any dependencies which have
    subproject fallbacks available, and will *only* use subprojects for
    them. This is useful when you want to test your fallback setup, or
    want to specifically build against the library sources provided by
    your subprojects.

* **--force-fallback-for=list,of,dependencies**

    Meson will not look at the system for any dependencies listed there,
    provided a fallback was supplied when the dependency was declared.

    This option takes precedence over `--wrap-mode=nofallback`, and when
    used in combination with `--wrap-mode=nodownload` will only work
    if the dependency has already been downloaded.

    This is useful when your project has many fallback dependencies,
    but you only want to build against the library sources for a few
    of them.

    **Warning**: This could lead to mixing system and subproject version of the
    same library in the same process. Take this case as example:
    - Libraries `glib-2.0` and `gstreamer-1.0` are installed on your system.
    - `gstreamer-1.0` depends on `glib-2.0`, pkg-config file `gstreamer-1.0.pc`
      has `Requires: glib-2.0`.
    - In your application build definition you do:
      ```meson
      executable('app', ...,
        dependencies: [
          dependency('glib-2.0', fallback: 'glib'),
          dependency('gstreamer-1.0', fallback: 'gstreamer')],
      )
      ```
    - You configure with `--force-fallback-for=glib`.
    This result in linking to two different versions of library `glib-2.0`
    because `dependency('glib-2.0', fallback: 'glib')` will return the
    subproject dependency, but `dependency('gstreamer-1.0', fallback: 'gstreamer')`
    will not fallback and return the system dependency, including `glib-2.0`
    library. To avoid that situation, every dependency that itself depend on
    `glib-2.0` must also be forced to fallback, in this case with
    `--force-fallback-for=glib,gsteamer`.

* **--wrap-mode=nopromote**

    *Since 0.56.0* Meson will automatically use wrap files found in subprojects
    and copy them into the main project. That new behavior can be disabled by
    passing `--wrap-mode=nopromote`. In that case only wraps found in the main
    project will be used.

## `meson subprojects` command

*Since 0.49.0*

`meson subprojects` has various subcommands to manage all subprojects.
If the subcommand fails on any subproject the execution continues with
other subprojects. All subcommands accept `--sourcedir` argument
pointing to the root source dir of the main project.

*Since 0.56.0* all subcommands accept `--types <file|git|hg|svn>`
argument to run the subcommands only on subprojects of the given
types. Multiple types can be set as comma separated list e.g. `--types
git,file`.

*Since 0.56.0* If the subcommand fails on any subproject an error code
is returned at the end instead of retuning success.

### Download subprojects

*Since 0.49.0*

Meson will automatically download needed subprojects during configure,
unless **--wrap-mode=nodownload** option is passed. It is sometimes
preferable to download all subprojects in advance, so the Meson
configure can be performed offline. The command-line `meson
subprojects download` can be used for that, it will download all
missing subprojects, but will not update already fetched subprojects.

### Update subprojects

*Since 0.49.0*

Once a subproject has been fetched, Meson will not update it automatically.
For example if the wrap file tracks a git branch, it won't pull latest commits.

To pull latest version of all your subprojects at once, just run the command:
`meson subprojects update`.
- If the wrap file comes from wrapdb, the latest version of the wrap file will
  be pulled and used next time meson reconfigure the project. This can be
  triggered using `meson --reconfigure`. Previous source tree is not deleted, to
  prevent from any loss of local changes. *Since 0.58.0* If `--reset` is
  specified, the source tree is deleted and new source is extracted.
- If subproject is currently in detached mode, a checkout of the revision from
  wrap file is performed. *Since 0.56.0* a rebase is also performed in case the
  revision already existed locally but was outdated. If `--reset` is specified,
  a hard reset is performed instead of rebase.
- If subproject is currently at the same branch as specified by the wrap file,
  a rebase on `origin` commit is performed. *Since 0.56.0* If `--reset` is
  specified, a hard reset is performed instead of rebase.
- If subproject is currently in a different branch as specified by the wrap file,
  it is skipped unless `--rebase` option is passed in which case a rebase on
  `origin` commit is performed. *Since 0.56.0* the `--rebase` argument is
  deprecated and has no effect. Instead, a checkout of the revision from wrap file
  file is performed and a rebase is also performed in case the revision already
  existed locally by was outdated. If `--reset` is specified, a hard reset is
  performed instead of rebase.
- *Since 0.56.0* if the `url` specified in wrap file is different to the URL set
  on `origin` for a git repository it will not be updated, unless `--reset` is
  specified in which case the URL of `origin` will be reset first.
- *Since 0.56.0* If the subproject directory is not a git repository but has a
  `[wrap-git]` the subproject is ignored, unless `--reset` is specified in which
  case the directory is deleted and the new repository is cloned.

### Start a topic branch across all git subprojects

*Since 0.49.0*

The command-line `meson subprojects checkout <branch_name>` will
checkout a branch, or create one with `-b` argument, in every git
subprojects. This is useful when starting local changes across
multiple subprojects. It is still your responsibility to commit and
push in each repository where you made local changes.

To come back to the revision set in wrap file (i.e. master), just run
`meson subprojects checkout` with no branch name.

*Since 0.56.0* any pending changes are now stashed before checkout a new branch.

### Execute a command on all subprojects

*Since 0.51.0*

The command-line `meson subprojects foreach <command> [...]` will
execute a command in each subproject directory. For example this can
be useful to check the status of subprojects (e.g. with `git status`
or `git diff`) before performing other actions on them.

## Why must all subprojects be inside a single directory?

There are several reasons.

First of all, to maintain any sort of sanity, the system must prevent going
inside other subprojects with `subdir()` or variations thereof. Having the
subprojects in well defined places makes this easy. If subprojects could be
anywhere at all, it would be a lot harder.

Second of all it is extremely important that end users can easily see what
subprojects any project has. Because they are in one, and only one, place,
reviewing them becomes easy.

This is also a question of convention. Since all Meson projects have the same
layout w.r.t subprojects, switching between projects becomes easier. You don't
have to spend time on a new project traipsing through the source tree looking
for subprojects. They are always in the same place.

Finally if you can have subprojects anywhere, this increases the possibility of
having many different (possibly incompatible) versions of a dependency in your
source tree. Then changing some code (such as changing the order you traverse
directories) may cause a completely different version of the subproject to be
used by accident.
