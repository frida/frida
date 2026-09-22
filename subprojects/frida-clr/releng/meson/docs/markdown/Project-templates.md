---
short-description: Project templates
...

# Project templates

To make it easier for new developers to start working, Meson ships a
tool to generate the basic setup of different kinds of projects. This
functionality can be accessed with the `meson init` command. A typical
project setup would go like this:

```console
$ mkdir project_name
$ cd project_name
$ meson init --language=c --name=myproject --version=0.1
```

This would create the build definitions for a helloworld type
project. The result can be compiled as usual. For example it
could be done like this:

```
$ meson setup builddir
$ meson compile -C builddir
```

The generator has many different projects and settings. They can all
be listed by invoking the command `meson init --help`.

This feature is available since Meson version 0.45.0.

# Generate a build script for an existing project

With `meson init` you can generate a build script for an existing
project with existing project files by running the command in the
root directory of your project. Meson currently supports this
feature for `executable`, and `jar` projects.

# Build after generation of template

It is possible to have Meson generate a build directory from the
`meson init` command without running `meson setup`. This is done
by passing `-b` or `--build` switch.

```console
$ mkdir project_name
$ cd project_name
$ meson init --language=c --name=myproject --version=0.1 --build
```