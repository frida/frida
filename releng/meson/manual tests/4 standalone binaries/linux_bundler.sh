#!/bin/sh -eu

libdir="${MESON_INSTALL_PREFIX}/lib"
mkdir -p $libdir
sdlfile=`ldd ${MESON_INSTALL_PREFIX}/bin/myapp | grep libSDL | cut -d ' ' -f 3`
cp $sdlfile "${libdir}"
strip "${libdir}/libSDL"*
