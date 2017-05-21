#!/bin/sh

machine=$(uname -m)
case $machine in
  i?86)
    echo x86;
    ;;
  *)
    echo $machine
    ;;
esac
