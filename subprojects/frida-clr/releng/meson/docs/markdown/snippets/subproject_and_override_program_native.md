## subproject() and meson.override_find_program() now support the native keyword argument

Subprojects may now be built for the host or build machine (or both). This means
that build time dependencies can be built for the matching running the build in a
cross compile setup. When a subproject is run for the build machine it will act
just like a normal build == host setup, except that no targets will be installed.

This necessarily means that `meson.override_find_program()` must differentiate
between programs for the host and those for the build machine, as you may need
two versions of the same program, which have different outputs based on the
machine they are for. For backwards compatibility reasons the default is for the
host machine. Inside a native subproject the host and build machine will both be
the build machine.
