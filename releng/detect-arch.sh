#!/bin/sh

if [ "$(uname -s)" = "Darwin" ]; then
  if [ "$(sysctl -nq hw.optional.arm64)" = "1" ]; then
    machine=arm64
  else
    machine=x86_64
  fi
else
  machine=$(uname -m)
  case $machine in
    i?86)
      machine=x86
      ;;
    armv*)
      machine=armhf
      ;;
    aarch64)
      machine=arm64
      ;;
  esac
fi

echo $machine
