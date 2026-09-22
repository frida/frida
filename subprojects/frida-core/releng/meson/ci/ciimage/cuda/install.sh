#!/bin/bash

set -e

source /ci/common.sh

pkgs=(
  python python-pip python-setuptools
  ninja gcc gcc-objc git cmake
  cuda zlib pkgconf
)

PACMAN_OPTS='--needed --noprogressbar --noconfirm'

pacman -Syu $PACMAN_OPTS "${pkgs[@]}"
install_minimal_python_packages

# Manually remove cache to avoid GitHub space restrictions
rm -rf /var/cache/pacman

echo "source /etc/profile.d/cuda.sh" >> /ci/env_vars.sh
