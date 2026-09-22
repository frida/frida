#!/bin/sh -eu

curdir=`pwd`
rm -rf buildtmp
mkdir buildtmp
LDFLAGS=-static-libstdc++ ~/meson/meson.py buildtmp --buildtype=release --prefix=/tmp/myapp --libdir=lib --strip
ninja -C buildtmp install
rm -rf buildtmp
cd /tmp/
tar czf myapp.tar.gz myapp
mv myapp.tar.gz "$curdir"
rm -rf myapp
