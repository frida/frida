#!/bin/sh -eu

mkdir -p ${MESON_INSTALL_PREFIX}/Contents/Frameworks
cp -R /Library/Frameworks/SDL2.framework ${MESON_INSTALL_PREFIX}/Contents/Frameworks

install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 @executable_path/../Frameworks/SDL2.framework/Versions/A/SDL2 ${MESON_INSTALL_PREFIX}/Contents/MacOS/myapp
