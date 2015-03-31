#!/bin/sh

machine=$(uname -m)
case $machine in
  i?86)
    echo i386;
    ;;
  *)
    echo $machine
    ;;
esac
