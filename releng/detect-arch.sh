#!/bin/sh

machine=$(uname -m)
case $machine in
  i?86)
    echo x86;
    ;;
  arm64)
    [ "$(uname -s)" == "Darwin" ] && echo arm64e || echo arm64
    ;;
  aarch64)
    echo arm64;
    ;;
  *)
    echo $machine
    ;;
esac
