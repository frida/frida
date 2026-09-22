#!/bin/sh

arch=arm
apex_libdirs="/apex/com.android.art/lib:/apex/com.android.runtime/lib"

exec $(dirname "$0")/run-android.sh $arch $apex_libdirs "$@"
