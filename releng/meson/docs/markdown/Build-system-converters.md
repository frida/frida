---
short-description: Converting other build systems to Meson
...

# Build system converters

Moving from one build system into another includes a fair bit of
work. To make things easier, Meson provides scripts to convert other
build systems into Meson. At the time of writing, scripts for CMake
and autotools exist. It can be found in the `tools` subdirectory in
Meson's source tree.

The scripts do not try to do a perfect conversion. This would be
extremely difficult because the data models of other build systems are
very different. The goal of the converter script is to convert as much
of the low level drudgery as possible. Using the scripts is
straightforward. We'll use the CMake one as an example but the
Autotools one works exactly the same way.

    cmake2meson.py path/to/CMake/project/root

This command generates a skeleton Meson project definition that tries
to mirror CMake's setup as close as possible. Once this is done, you
need to go through these files manually and finalize the
conversion. To make this task as simple as possible, the converter
script will transfer all comments from the CMake definition into Meson
definition.
