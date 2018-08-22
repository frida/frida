#!/bin/sh

# This wrapper script is necessary in order to combat g++ trying to be smart. We need to
# statically link libstdc++ into frida-core/lib/agent, which we haven't been able to find
# a way to do via conventional methods.

arguments="$@"
# we want to link against libstdc++.a:
arguments=$(echo $arguments | sed "s,/libstdc++.so,/libstdc++.a,")

$arguments
