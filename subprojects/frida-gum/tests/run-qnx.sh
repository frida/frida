#!/bin/bash

runner_tarball=$(mktemp "/tmp/gum-tests.XXXXXX")

function dispose {
  rm -f "$runner_tarball"
}
trap dispose EXIT

set -ex

make

cd build/tests
tar -cf "$runner_tarball" gum-tests data/
/opt/sabrelite/run.sh "$runner_tarball" /opt/frida/gum-tests
