#!/bin/sh

arch=$1
apex_libdirs=$2

remote_prefix=/data/local/tmp/frida-tests-$arch

set -e
make
cd build/tests
adb shell "mkdir $remote_prefix 2>/dev/null || true"
adb push gum-tests data $remote_prefix
adb shell "LD_LIBRARY_PATH='$apex_libdirs' $remote_prefix/gum-tests $@"
