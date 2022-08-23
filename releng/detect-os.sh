#!/bin/sh

if [ -n "$FRIDA_BUILD_OS" ]; then
  echo $FRIDA_BUILD_OS
  exit 0
fi

echo $(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,macos,')
