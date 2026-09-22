#!/bin/bash

set -e

source /ci/common.sh

pkgs=(
  python3-pip python3 python3-devel
  ninja make git autoconf automake patch libjpeg-devel
  elfutils gcc gcc-c++ gcc-fortran gcc-objc gcc-obj-c++ vala rust bison flex curl lcov
  mono-core gtkmm3-devel gtest gmock protobuf-devel wxGTK3-3_2-devel gobject-introspection-devel
  itstool gtk3-devel java-17-openjdk-devel gtk-doc llvm-devel clang-devel libSDL2-devel graphviz-devel zlib-devel zlib-devel-static
  #hdf5-devel netcdf-devel libscalapack2-openmpi3-devel libscalapack2-gnu-openmpi3-hpc-devel openmpi3-devel
  doxygen vulkan-devel vulkan-validationlayers openssh mercurial gtk-sharp3-complete gtk-sharp2-complete libpcap-devel libgpgme-devel
  libqt5-qtbase-devel libqt5-qttools-devel libqt5-linguist libqt5-qtbase-private-headers-devel
  libwmf-devel valgrind cmake nasm gnustep-base-devel gettext-tools gettext-runtime gettext-csharp ncurses-devel
  libxml2-devel libxslt-devel libyaml-devel glib2-devel json-glib-devel
  boost-devel libboost_date_time-devel libboost_filesystem-devel libboost_locale-devel libboost_system-devel
  libboost_test-devel libboost_log-devel libboost_regex-devel
  libboost_python3-devel libboost_regex-devel
)

# Sys update
zypper --non-interactive patch --with-update --with-optional
zypper --non-interactive update

# Install deps
zypper install -y "${pkgs[@]}"
install_python_packages hotdoc

echo 'export PKG_CONFIG_PATH="/usr/lib64/mpi/gcc/openmpi3/lib64/pkgconfig:$PKG_CONFIG_PATH"' >> /ci/env_vars.sh

# dmd is very special on OpenSUSE (as in the packages do not work)
# see https://bugzilla.opensuse.org/show_bug.cgi?id=1162408
curl -fsS https://dlang.org/install.sh | bash -s dmd | tee dmd_out.txt
cat dmd_out.txt | grep source | sed 's/^[^`]*`//g' | sed 's/`.*//g' >> /ci/env_vars.sh
chmod +x /ci/env_vars.sh

source /ci/env_vars.sh

dub_fetch urld
dub build urld --compiler=dmd
dub_fetch dubtestproject
dub build dubtestproject:test1 --compiler=dmd
dub build dubtestproject:test2 --compiler=dmd

# Cleanup
zypper --non-interactive clean --all
