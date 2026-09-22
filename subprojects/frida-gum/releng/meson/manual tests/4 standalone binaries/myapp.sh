#!/bin/bash

cd "${0%/*}"

if [ `uname` == 'Darwin' ]; then
    ./myapp
else
    export LD_LIBRARY_PATH="`pwd`/lib"
    bin/myapp
fi
