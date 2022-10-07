#!/bin/bash

if [ -z "$FRIDA_VERSION" ]; then
  echo "FRIDA_VERSION must be set" > /dev/stderr
  exit 1
fi

set -e

cd build/release-assets
for name in *; do
  if echo $name | grep -q $FRIDA_VERSION; then
    continue
  fi
  case $name in
    frida-*-devkit-*)
      new_name=$(echo $name | sed -e "s,devkit-,devkit-$FRIDA_VERSION-,")
      ;;
    frida-server-*|frida-portal-*|frida-inject-*|frida-gadget-*|frida-swift-*|frida-clr-*|frida-qml-*|gum-graft-*)
      new_name=$(echo $name | sed -E -e "s,^(frida|gum)-([^-]+),\\1-\\2-$FRIDA_VERSION,")
      ;;
    *)
      new_name=""
      ;;
  esac
  if [ -n "$new_name" ]; then
    mv -v $name $new_name
  fi
done
