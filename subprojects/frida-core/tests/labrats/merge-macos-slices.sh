#!/bin/sh

set -ex

for f in *-macos*; do
  echo "Patching $f"
  lipo ../$f -thin i386 -output $f.i386
  lipo ../$f -thin x86_64 -output $f.x86_64
  if file $f | grep -q universal; then
    lipo $f -thin arm64 -output $f.arm64
    lipo $f -thin arm64e -output $f.arm64e
    lipo $f.i386 $f.x86_64 $f.arm64 $f.arm64e -create -output ../$f
    rm $f.i386 $f.x86_64 $f.arm64 $f.arm64e
  else
    lipo $f.i386 $f.x86_64 $f -create -output ../$f
    rm $f.i386 $f.x86_64
  fi
done
