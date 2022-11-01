#!/bin/sh

set -e

[ -z "$1" ] && exit 1
build_machine=$1

releng_path=`dirname $0`
cd $releng_path/../
FRIDA_ROOT=`pwd`
cd -
FRIDA_BUILD="${FRIDA_BUILD:-$FRIDA_ROOT/build}"
MAKE="${MAKE:-make}"

if ! meson --version >/dev/null 2>&1; then
  echo "Meson not found" > /dev/stderr
  exit 1
fi

if ! ninja --version >/dev/null 2>&1; then
  echo "Ninja not found" > /dev/stderr
  exit 1
fi

if ! pkg-config --version >/dev/null 2>&1; then
  echo "pkg-config not found" > /dev/stderr
  exit 1
fi

if ! valac --version >/dev/null 2>&1; then
  echo "Vala compiler not found" > /dev/stderr
  exit 1
fi

"$MAKE" -f Makefile.toolchain.mk deps/.vala-stamp

srcdir="$FRIDA_ROOT/deps/vala"
builddir="$FRIDA_BUILD/ft-tmp-$build_machine/bootstrap"

rm -rf "$builddir"
mkdir -p "$builddir"
cd "$builddir"
meson setup \
  --prefix / \
  --default-library static \
  . \
  "$srcdir"
meson compile
DESTDIR="$builddir/dist" meson install

cd "$builddir/dist"
ln -s "$(which ninja)" bin/ninja
ln -s "$(which pkg-config)" bin/pkg-config
mkdir -p share/aclocal
tar -cjf "$FRIDA_BUILD/_toolchain-$build_machine.tar.bz2" .
