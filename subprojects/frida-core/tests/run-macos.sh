#!/bin/sh

arch=x86_64

frida_tests=$(dirname "$0")
cd "$frida_tests/../../build/tmp-macos-$arch/frida-core" || exit 1
. ../../frida-env-macos-x86_64.rc
ninja || exit 1
tests/frida-tests "$@"
