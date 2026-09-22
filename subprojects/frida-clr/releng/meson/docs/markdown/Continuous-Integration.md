# Continuous Integration

Here you will find snippets to use Meson with various CI such as
Travis and AppVeyor.

Please [file an issue](https://github.com/mesonbuild/meson/issues/new)
if these instructions don't work for you.

## Travis-CI with Docker

Travis with Docker gives access to newer non-LTS Ubuntu versions with
pre-installed libraries of your choice.

This `yml` file is derived from the
[configuration used by Meson](https://github.com/mesonbuild/meson/blob/master/.travis.yml)
for running its own tests.

```yaml
os:
  - linux
  - osx

language:
  - cpp

services:
  - docker

before_install:
  - if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then brew update; fi
  - if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then brew install python3 ninja; fi
  - if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then pip3 install meson; fi
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then docker pull YOUR/REPO:eoan; fi

script:
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then echo FROM YOUR/REPO:eoan > Dockerfile; fi
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then echo ADD . /root >> Dockerfile; fi
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then docker build -t withgit .; fi
  - if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then docker run withgit /bin/sh -c "cd /root && TRAVIS=true CC=$CC CXX=$CXX meson setup builddir && meson test -C builddir"; fi
  - if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then SDKROOT=$(xcodebuild -version -sdk macosx Path) meson setup builddir && meson test -C builddir; fi
```

## CircleCi for Linux (with Docker)

[CircleCi](https://circleci.com/) can work for spinning all of the
Linux images you wish. Here's a sample `yml` file for use with that.

```yaml
version: 2.1

executors:
  # Your dependencies would go in the docker images that represent
  # the Linux distributions you are supporting
  meson_ubuntu_builder:
    docker:
      - image: your_dockerhub_username/ubuntu-sys

  meson_debian_builder:
    docker:
      - image: your_dockerhub_username/debian-sys

  meson_fedora_builder:
    docker:
      - image: your_dockerhub_username/fedora-sys

jobs:
  meson_ubuntu_build:
    executor: meson_ubuntu_builder
    steps:
      - checkout
      - run: meson setup builddir --backend ninja
      - run: meson compile -C builddir
      - run: meson test -C builddir

  meson_debian_build:
    executor: meson_debian_builder
    steps:
      - checkout
      - run: meson setup builddir --backend ninja
      - run: meson compile -C builddir
      - run: meson test -C builddir

  meson_fedora_build:
    executor: meson_fedora_builder
    steps:
      - checkout
      - run: meson setup builddir --backend ninja
      - run: meson compile -C builddir
      - run: meson test -C builddir

workflows:
  version: 2
  linux_workflow:
    jobs:
      - meson_ubuntu_build
      - meson_debian_build
      - meson_fedora_build
```

## AppVeyor for Windows

For CI on Windows, [AppVeyor](https://www.appveyor.com/) has a wide
selection of [default
configurations](https://www.appveyor.com/docs/windows-images-software/).
AppVeyor also has
[MacOS](https://www.appveyor.com/docs/macos-images-software/) and
[Linux](https://www.appveyor.com/docs/linux-images-software/) CI
images. This is a sample `appveyor.yml` file for Windows with Visual
Studio 2015 and 2017.

```yaml
image: Visual Studio 2017

environment:
  matrix:
    - arch: x86
      compiler: msvc2015
    - arch: x64
      compiler: msvc2015
    - arch: x86
      compiler: msvc2017
    - arch: x64
      compiler: msvc2017

platform:
  - x64

install:
  # Set paths to dependencies (based on architecture)
  - cmd: if %arch%==x86 (set PYTHON_ROOT=C:\python37) else (set PYTHON_ROOT=C:\python37-x64)
  # Print out dependency paths
  - cmd: echo Using Python at %PYTHON_ROOT%
  # Add necessary paths to PATH variable
  - cmd: set PATH=%cd%;%PYTHON_ROOT%;%PYTHON_ROOT%\Scripts;%PATH%
  # Install meson and ninja
  - cmd: pip install ninja meson
  # Set up the build environment
  - cmd: if %compiler%==msvc2015 ( call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch% )
  - cmd: if %compiler%==msvc2017 ( call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %arch% )

build_script:
  - cmd: echo Building on %arch% with %compiler%
  - cmd: meson --backend=ninja builddir
  - cmd: meson compile -C builddir

test_script:
  - cmd: meson test -C builddir
```

### Qt

For Qt 5, add the following line near the `PYTHON_ROOT` assignment:

```yaml
 - cmd: if %arch%==x86 (set QT_ROOT=C:\Qt\5.11\%compiler%) else (set QT_ROOT=C:\Qt\5.11\%compiler%_64)
```

And afterwards add `%QT_ROOT%\bin` to the `PATH` variable.

You might have to adjust your build matrix as there are, for example,
no msvc2017 32-bit builds. Visit the [Build
Environment](https://www.appveyor.com/docs/build-environment/) page in
the AppVeyor docs for more details.

### Boost

The following statement is sufficient for Meson to find Boost:

```yaml
 - cmd: set BOOST_ROOT=C:\Libraries\boost_1_67_0
```

## Travis without Docker

Non-Docker Travis-CI builds can use Linux, MacOS or Windows.
Set the desired compiler(s) in the build **matrix**.
This example is for **Linux** (Ubuntu 18.04) and **C**.

```yaml
dist: bionic
group: travis_latest

os: linux
language: python

matrix:
  include:
    - env: CC=gcc
    - env: CC=clang

install:
  - pip install meson ninja

script:
  - meson setup builddir
  - meson compile -C builddir
  - meson test -C builddir
```

## GitHub Actions

GitHub Actions are distinct from Azure Pipelines in their workflow
syntax. It can be easier to setup specific CI tasks in Actions than
Pipelines, depending on the particular task. This is an example file:
.github/workflows/ci_meson.yml supposing the project is C-based, using
GCC on Linux, Mac and Windows. The optional `on:` parameters only run
this CI when the C code is changed--corresponding ci_python.yml might
run only on "**.py" file changes.

```yaml
name: ci_meson

on:
  push:
    paths:
      - "**.c"
      - "**.h"
  pull_request:
    paths:
      - "**.c"
      - "**.h"

jobs:

  linux:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v1
    - uses: actions/setup-python@v1
      with:
        python-version: '3.x'
    - run: pip install meson ninja
    - run: meson setup builddir/
      env:
        CC: gcc
    - run: meson test -C builddir/ -v
    - uses: actions/upload-artifact@v1
      if: failure()
      with:
        name: Linux_Meson_Testlog
        path: builddir/meson-logs/testlog.txt

  macos:
    runs-on: macos-latest
    steps:
    - uses: actions/checkout@v1
    - uses: actions/setup-python@v1
      with:
        python-version: '3.x'
    - run: brew install gcc
    - run: pip install meson ninja
    - run: meson setup builddir/
      env:
        CC: gcc
    - run: meson test -C builddir/ -v
    - uses: actions/upload-artifact@v1
      if: failure()
      with:
        name: MacOS_Meson_Testlog
        path: builddir/meson-logs/testlog.txt

  windows:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v1
    - uses: actions/setup-python@v1
      with:
        python-version: '3.x'
    - run: pip install meson ninja
    - run: meson setup builddir/
      env:
        CC: gcc
    - run: meson test -C builddir/ -v
    - uses: actions/upload-artifact@v1
      if: failure()
      with:
        name: Windows_Meson_Testlog
        path: builddir/meson-logs/testlog.txt
```
