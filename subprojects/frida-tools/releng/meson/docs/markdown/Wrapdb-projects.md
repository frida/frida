# Meson WrapDB packages

This is a list of projects that have either an upstream Meson build system, or a
port maintained by the Meson team. [They can be used by your project to provide
its dependencies](Wrap-dependency-system-manual.md).

Use the command line `meson wrap install <project>` to install the wrap file of
any of those projects into your project's `subprojects/` directory.
See [Meson command line documentation](Using-wraptool.md).

If you wish to add your own project into this list, please submit your wrap file
in a [Pull Request](https://github.com/mesonbuild/wrapdb).
See [Meson documentation](Adding-new-projects-to-wrapdb.md)
for more details.

{{ wrapdb-table.md }}
