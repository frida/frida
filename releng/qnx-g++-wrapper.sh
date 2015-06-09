#!/bin/sh

# This wrapper script is necessary in order to combat the evil that is libtool. We need to
# statically link libstdc++ into frida-core/lib/agent, which we haven't been able to find
# a way to do via conventional methods.

arguments="$@"
# we want all the symbols from libgcc:
arguments=$(echo $arguments | sed 's/-lgcc/-Wl,--whole-archive -lgcc -Wl,--no-whole-archive/')
# and we want to link against the libstdc++.a:
arguments=$(echo $arguments | sed "s,-lstdc++,$QNX_TARGET/armle/lib/gcc/4.8.3/libstdc++.a,")

$arguments
