This directory shows how you can build redistributable binaries. On
OSX this means building an app bundle and a .dmg installer. On Linux
it means building an archive that bundles its dependencies. On Windows
it means building an .exe installer.

To build each package you run the corresponding build_ARCH.sh build
script.

On Linux you must build the package on the oldest distribution you
plan to support (Debian stable/oldstable and old CentOS are the common
choice here).
