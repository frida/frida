#!/bin/bash

###
### Common functions for CI builder files.
### All functions can be accessed in install.sh via:
###
### $ source /ci/common.sh
###

set -e
set -x

base_python_pkgs=(
  pytest
  pytest-xdist
  pytest-subtests
  coverage
  fastjsonschema
)

python_pkgs=(
  cython
  gobject
  PyGObject
  lxml
  gcovr
)

dub_fetch() {
  set +e
  for (( i=1; i<=24; ++i )); do
    dub fetch "$@"
    (( $? == 0 )) && break

    echo "Dub Fetch failed. Retrying in $((i*5))s"
    sleep $((i*5))
  done
  set -e
}

install_minimal_python_packages() {
  rm -f /usr/lib*/python3.*/EXTERNALLY-MANAGED
  python3 -m pip install "${base_python_pkgs[@]}" $*
}

install_python_packages() {
  rm -f /usr/lib*/python3.*/EXTERNALLY-MANAGED
  python3 -m pip install "${base_python_pkgs[@]}" "${python_pkgs[@]}" $*
}
