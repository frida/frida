#!/bin/sh

arch=arm64
apex_libdirs="/apex/com.android.art/lib64:/apex/com.android.runtime/lib64"

exec $(dirname "$0")/run-android.sh $arch $apex_libdirs "$@"
