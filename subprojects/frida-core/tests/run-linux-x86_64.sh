#!/bin/sh

arch=x86_64

frida_tests=$(dirname "$0")
cd "$frida_tests/../../build/tmp_thin-linux-$arch/frida-core" || exit 1
. ../../frida_thin-env-linux-x86_64.rc
ninja || exit 1
tests/frida-tests "$@"
