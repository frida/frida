#!/bin/bash

host_os=$1

case $host_os in
  ios)
    device="ios-12.5.7-arm64"
    ;;
  android)
    device="android-8.1.0-arm64"
    ;;
  *)
    echo "Usage: $0 ios|android" > /dev/stderr
    exit 1
esac

if [ -z "$GH_TOKEN" ]; then
  echo "Missing GH_TOKEN environment variable" > /dev/stderr
  exit 1
fi

build_os=$(echo $(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,macos,'))
build_arch=$(uname -m)

runner_tarball=$(mktemp -t gum-tests.XXXXXX)
runner_script=$(mktemp -t gum-tests.XXXXXX)

function dispose {
  rm -f "$runner_tarball"
  rm -f "$runner_script"
}
trap dispose EXIT

set -ex

make

cd build/tests
tar czf "$runner_tarball" gum-tests data/

case $host_os in
  ios)
    cat << EOF > "$runner_script"
cd /usr/local
rm -rf opt/frida
mkdir -p opt/frida
cd opt/frida
tar xf \$ASSET_PATH
./gum-tests
EOF
    ;;
  android)
    cat << EOF > "$runner_script"
cd /data/local/tmp
tar xf \$ASSET_PATH
./gum-tests
EOF
    ;;
esac

curl \
    https://corellium.frida.re/devices/$device \
    --form "asset=@$runner_tarball" \
    --form "script=<$runner_script" \
    --form-string $'marker=\n*** Finished with exit code: ' \
    --form-string "token=$GH_TOKEN" \
    -N \
    -v
