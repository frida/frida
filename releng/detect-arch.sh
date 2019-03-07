#!/bin/sh

machine=$(uname -m)
case $machine in
  i?86)
    echo x86;
    ;;
  aarch64)
    echo arm64;
    ;;
  *)
    echo $machine
    ;;
esac
