#!/bin/sh

remote_prefix=/data/local/tmp/frida-core-tests

set -e

core_tests=$(dirname "$0")
cd "$core_tests/../"
make
cd build/tests
adb shell "mkdir -p $remote_prefix"
adb push frida-tests labrats ../lib/agent/frida-agent.so $remote_prefix
adb shell "su -c '$remote_prefix/frida-tests $@'"
