<p align="center">
<img src="https://mesonbuild.com/assets/images/meson_logo.png">
</p>
Meson® is a project to create the best possible next-generation
build system.

#### Status

[![PyPI](https://img.shields.io/pypi/v/meson.svg)](https://pypi.python.org/pypi/meson)
[![Build Status](https://dev.azure.com/jussi0947/jussi/_apis/build/status/mesonbuild.meson)](https://dev.azure.com/jussi0947/jussi/_build/latest?definitionId=1)
[![Codecov](https://codecov.io/gh/mesonbuild/meson/coverage.svg?branch=master)](https://codecov.io/gh/mesonbuild/meson/branch/master)

#### Dependencies

 - [Python](https://python.org) (version 3.7 or newer)
 - [Ninja](https://ninja-build.org) (version 1.8.2 or newer)

Latest Meson version supporting previous Python versions:
- Python 3.6: **0.61.5**
- Python 3.5: **0.56.2**
- Python 3.4: **0.45.1**

#### Installing from source

Meson is available on [PyPi](https://pypi.python.org/pypi/meson), so
it can be installed with `pip3 install meson`.  The exact command to
type to install with `pip` can vary between systems, be sure to use
the Python 3 version of `pip`.

If you wish you can install it locally with the standard Python command:

```console
python3 -m pip install meson
```

For builds using Ninja, Ninja can be downloaded directly from Ninja
[GitHub release page](https://github.com/ninja-build/ninja/releases)
or via [PyPi](https://pypi.python.org/pypi/ninja)

```console
python3 -m pip install ninja
```

More on Installing Meson build can be found at the
[getting meson page](https://mesonbuild.com/Getting-meson.html).

#### Creating a standalone script

Meson can be run as a [Python zip
app](https://docs.python.org/3/library/zipapp.html). To generate the
executable run the following command:

    ./packaging/create_zipapp.py --outfile meson.pyz --interpreter '/usr/bin/env python3' <source checkout>

#### Running

Meson requires that you have a source directory and a build directory
and that these two are different. In your source root must exist a
file called `meson.build`. To generate the build system run this
command:

`meson setup <source directory> <build directory>`

Depending on how you obtained Meson the command might also be called
`meson.py` instead of plain `meson`. In the rest of this document we
are going to use the latter form.

You can omit either of the two directories, and Meson will substitute
the current directory and autodetect what you mean. This allows you to
do things like this:

```console
cd <source root>
meson setup builddir
```

To compile, cd into your build directory and type `ninja`. To run unit
tests, type `ninja test`.

More on running Meson build system commands can be found at the
[running meson page](https://mesonbuild.com/Running-Meson.html)
or by typing `meson --help`.

#### Contributing

We love code contributions. See the [contribution
page](https://mesonbuild.com/Contributing.html) on the website for
details.


#### IRC

The channel to use is `#mesonbuild` either via Matrix ([web
interface][matrix_web]) or [OFTC IRC][oftc_irc].

[matrix_web]: https://app.element.io/#/room/#mesonbuild:matrix.org
[oftc_irc]: https://www.oftc.net/

#### Further info

More information about the Meson build system can be found at the
[project's home page](https://mesonbuild.com).

Meson is a registered trademark of ***Jussi Pakkanen***.
