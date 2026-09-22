#!/bin/sh -eu

rm -rf buildtmp
mkdir buildtmp
~/meson/meson.py buildtmp --buildtype=release  --prefix=/tmp/myapp.app --bindir=Contents/MacOS
ninja -C buildtmp install
rm -rf buildtmp
mkdir -p mnttmp
rm -f working.dmg
gunzip < template.dmg.gz > working.dmg
hdiutil attach working.dmg -noautoopen -quiet -mountpoint mnttmp
rm -rf mnttmp/myapp.app
mv /tmp/myapp.app mnttmp
# NOTE: output of hdiutil changes every now and then.
# Verify that this is still working.
hdiutil detach $(hdiutil info|grep "mnttmp"|awk '{print $1}')
rm -rf mnttmp
rm -f myapp.dmg
hdiutil convert working.dmg -quiet -format UDZO -imagekey zlib-level=9 -o myapp.dmg
rm -f working.dmg
