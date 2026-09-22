#!/bin/sh

arch=x86_64
apex_libdirs=/apex/com.android.runtime/lib64

exec $(dirname "$0")/run-android.sh $arch $apex_libdirs "$@"
