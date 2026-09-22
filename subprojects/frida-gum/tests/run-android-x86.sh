#!/bin/sh

arch=x86
apex_libdirs=/apex/com.android.runtime/lib

exec $(dirname "$0")/run-android.sh $arch $apex_libdirs "$@"
