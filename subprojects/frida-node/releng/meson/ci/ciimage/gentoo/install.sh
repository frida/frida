#!/bin/bash
set -e

source /ci/common.sh

# We divide the package list into 'pkgs_stable' and 'pkgs_latest'. The trade-off
# is that latest stuff may not have a binpkg available, but of course we get
# better test coverage with the bleeding edge then.
pkgs_stable=(
  app-portage/portage-utils
  dev-build/cmake
  dev-vcs/git

  # language support
  dev-python/cython
  dev-python/lxml
  dev-python/pip
  virtual/fortran
  dev-lang/nasm
  dev-lang/vala
  dev-lang/python:2.7
  dev-java/openjdk-bin
  # requires rustfmt, bin rebuild (TODO: file bug)
  #dev-util/bindgen

  dev-libs/elfutils
  dev-libs/gobject-introspection
  dev-util/itstool
  dev-libs/protobuf

  # custom deps
  net-libs/libpcap
  dev-util/gtk-doc
  media-libs/libwmf
  sys-cluster/openmpi
  sci-libs/netcdf
  media-libs/libsdl2
  dev-cpp/gtest
  sci-libs/hdf5
  dev-qt/linguist-tools
  sys-devel/llvm
  # qt6 unstable
  #dev-qt/qttools

  # misc
  app-admin/sudo
  app-text/doxygen
  sys-apps/fakeroot
  sys-devel/bison
  sys-devel/gettext

  # TODO: vulkan-validation-layers
  # TODO: cuda
  #dev-cpp/gtkmm:3.0
  #dev-java/openjdk-bin:8
  #dev-lang/go
  #dev-lang/mono
  #dev-lang/python
  #dev-lang/rust-bin
  #dev-libs/wayland
  #dev-libs/wayland-protocols
  #dev-python/pypy3
  #dev-qt/qtbase:6
  #dev-qt/qtcore:5
  #dev-qt/qttools:6
  #dev-vcs/mercurial
  #gnustep-base/gnustep-base
  #media-gfx/graphviz
  #sci-libs/netcdf-fortran
  #sys-devel/clang
  #x11-libs/gtk+:3
)
pkgs_latest=(
  # ~arch boost needed for py3.12 for now (needs 1.84)
  dev-build/b2
  dev-libs/boost

  dev-build/autoconf
  dev-build/automake

  # ~arch only
  sci-libs/scalapack
)
pkgs=( "${pkgs_stable[@]}" "${pkgs_latest[@]}" )

emerge-webrsync --quiet

# This means we can't really take advantage of the binhost but a lot of the
# advantages of using Gentoo in CI come from the bleeding edge side.
# With full ~arch, we don't get binpkgs for much at all. Instead, let's just
# do ~arch for the test deps we have.
#echo 'ACCEPT_KEYWORDS="~amd64"' >> /etc/portage/make.conf

printf "%s\n" ${pkgs[@]} >> /var/lib/portage/world
printf "%s\n" ${pkgs_latest[@]} >> /etc/portage/package.accept_keywords/meson
cat /etc/portage/package.accept_keywords/meson

cat <<-EOF > /etc/portage/package.accept_keywords/misc
	dev-lang/python-exec
	dev-lang/python
EOF

mkdir /etc/portage/binrepos.conf || true
mkdir /etc/portage/profile || true
cat <<-EOF > /etc/portage/package.use/ci
	dev-cpp/gtkmm X

	dev-libs/boost python
	sys-libs/zlib static-libs
EOF

cat <<-EOF >> /etc/portage/make.conf
	EMERGE_DEFAULT_OPTS="--complete-graph --quiet=y --quiet-build=y --jobs=$(nproc) --load-average=$(nproc)"
	EMERGE_DEFAULT_OPTS="\${EMERGE_DEFAULT_OPTS} --autounmask-write --autounmask-continue --autounmask-keep-keywords=y --autounmask-use=y"
	EMERGE_DEFAULT_OPTS="\${EMERGE_DEFAULT_OPTS} --binpkg-respect-use=y"

	FEATURES="\${FEATURES} parallel-fetch parallel-install -merge-sync"
	FEATURES="\${FEATURES} getbinpkg binpkg-request-signature"

	# These don't work in Docker, so reduce noise in logs
	FEATURES="\${FEATURES} -ipc-sandbox -network-sandbox -pid-sandbox"
EOF

# TODO: Enable all Pythons / add multiple jobs with diff. Python impls?
#echo '*/* PYTHON_TARGETS: python3_10 python3_11 python3_12' >> /etc/portage/package.use/python
echo '*/* PYTHON_TARGETS: python3_12' >> /etc/portage/package.use/python
cat <<-EOF >> /etc/portage/profile/use.mask
-python_targets_python3_12
-python_single_target_python3_12
EOF
cat <<-EOF >> /etc/portage/profile/use.stable.mask
-python_targets_python3_12
-python_single_target_python3_12
EOF

echo 'dev-lang/python ensurepip' >> /etc/portage/package.use/python

# Silly mono circular dep
#USE=minimal emerge --oneshot dev-lang/mono

# If we don't want to do this, we could use the 'portage' container instead
# so the stage3/repo match.
emerge --update --deep --changed-use @world
qlop -d 'yesterday'

env-update && . /etc/profile

rm /usr/lib/python/EXTERNALLY-MANAGED
python3 -m ensurepip
install_python_packages
python3 -m pip install "${base_python_pkgs[@]}"
