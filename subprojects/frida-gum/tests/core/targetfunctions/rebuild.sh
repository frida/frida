#!/bin/bash

# This script is responsible for building Frida helpers for various Linux
# architectures. It can build helpers for a single specified architecture on the
# local machine, or for supported architectures in a container. The script uses
# Docker containers to ensure consistent build environments for each
# architecture.
#
# Note that the expectation is that when running the build for a specific
# architecture that it be run from inside the relevant container. This script is
# used by CI.

set -euo pipefail

CURRENT_FILE="${BASH_SOURCE[0]}"
TARGETFUNCTIONS_DIR="$(cd "$(dirname "$CURRENT_FILE")" && pwd)"
FRIDA_GUM_DIR="$(cd "$TARGETFUNCTIONS_DIR/../../.." && pwd)"
RELENG_DIR="$FRIDA_GUM_DIR/releng"
BUILD_DIR="$FRIDA_GUM_DIR/build"
RELATIVE_TO_FRIDA_GUM_DIR=$(realpath --relative-to="$FRIDA_GUM_DIR" "$CURRENT_FILE")

TMP_MESON_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_MESON_DIR"' EXIT

CONTAINER_REGISTRY="${CONTAINER_REGISTRY:-ghcr.io/frida}"

main () {
  if [ "$#" -eq 0 ]; then
    build_arches_in_container
    return
  fi

  if [ "$#" -gt 1 ]; then
    echo >&2 "Error: Too many arguments"
    usage
  fi

  build_arch "$1"
}

usage () {
  echo >&2 "Usage: $0 [<arch>]"
  echo >&2 "If no arch is specified, then all helpers will be built in the container."
  exit 1
}

ARCHS=(
  x86
  x86_64
  armhf
  armbe8
  arm64
  arm64be
  arm64beilp32
  mips
  mipsel
  mips64
  mips64el
)

build_arch () {
  ARCH=$1
  if [ -z "$ARCH" ]; then
    usage
  fi
  if ! printf '%s\n' "${ARCHS[@]}" | grep -qx "$ARCH"; then
    echo >&2 "Error: Invalid architecture '$ARCH'"
    echo >&2 "Supported architectures: ${ARCHS[*]}"
    exit 1
  fi

  EXTRA_FLAGS=()
  if [ "$ARCH" == "x86" ]; then
    EXTRA_FLAGS+=("--build=linux-x86")
    export CC="gcc -m32" CXX="g++ -m32" STRIP="strip"
  fi

  export FRIDA_HOST=linux-$ARCH

  cd "$FRIDA_GUM_DIR"

  rm -rf "$BUILD_DIR"
  # Note that $XTOOLS_HOST is set by the container.
  ./configure --host="$XTOOLS_HOST" "${EXTRA_FLAGS[@]}"
  make -C tests/core/targetfunctions
}

build_arches_in_container () {
  for ARCH in "${ARCHS[@]}"; do
    docker run \
      --rm \
      --name "x-tools-linux-$ARCH" \
      -w /frida-gum \
      -i -t \
      -v "$FRIDA_GUM_DIR:/frida-gum" \
      "$CONTAINER_REGISTRY/x-tools-linux-$ARCH:latest" \
      "/frida-gum/$RELATIVE_TO_FRIDA_GUM_DIR" "$ARCH"
  done
}

main "$@"
