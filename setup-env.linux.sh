#! /bin/bash

toolchain_version=20130209
sdk_version=20130212
build_os=linux

download_command="wget -O - -nv"
tar_stdin=""

if [ -z $FRIDA_TARGET ] ; then
  case $(uname -m) in
    x86_64)
      FRIDA_TARGET=linux-x86_64
    ;;
    i686)
      FRIDA_TARGET=linux-x86_32
    ;;
    *)
    echo "Could not automatically determine architecture" > /dev/stderr
    exit 1
  esac
fi

export FRIDA_ENVIRONMENT=normal
export FRIDA_ROOT="$(pwd)"
export FRIDA_BUILD="${FRIDA_ROOT}/build"
export FRIDA_PREFIX="${FRIDA_BUILD}/frida-${FRIDA_TARGET}"
export FRIDA_PREFIX_LIB="${FRIDA_PREFIX}/lib"
export FRIDA_TOOLROOT="${FRIDA_BUILD}/toolchain"
export FRIDA_SDKROOT="${FRIDA_BUILD}/sdk-${FRIDA_TARGET}"
export PATH="${FRIDA_TOOLROOT}/bin:${FRIDA_PREFIX}/bin:${PATH}"

export CC="/usr/bin/gcc-4.6"
export CXX="/usr/bin/g++-4.6"
export LD="/usr/bin/ld"
export CFLAGS="-ffunction-sections -fdata-sections"
export LDFLAGS="-Wl,--gc-sections"
export CFLAGS="-fPIC $CFLAGS"
export CXXFLAGS="$CFLAGS $CXXFLAGS"
export OBJCFLAGS="$CFLAGS"
export CPPFLAGS="$CFLAGS"

export ACLOCAL_FLAGS="-I ${FRIDA_PREFIX}/share/aclocal -I ${FRIDA_SDKROOT}/share/aclocal -I ${FRIDA_TOOLROOT}/share/aclocal"
export ACLOCAL="aclocal ${ACLOCAL_FLAGS}"
export CONFIG_SITE="${FRIDA_BUILD}/config.site"

PKG_CONFIG="${FRIDA_TOOLROOT}/bin/pkg-config --define-variable=frida_sdk_prefix=${FRIDA_SDKROOT}"
PKG_CONFIG_PATH="${FRIDA_PREFIX_LIB}/pkgconfig:${FRIDA_SDKROOT}/lib/pkgconfig"

VALAC="${FRIDA_TOOLROOT}/bin/valac-0.14 --vapidir=\"${FRIDA_TOOLROOT}/share/vala-0.14/vapi\""
VALAC="$VALAC --vapidir=\"${FRIDA_SDKROOT}/share/vala/vapi\" --vapidir=\"${FRIDA_PREFIX}/share/vala/vapi\""

[ ! -d "${FRIDA_PREFIX}/share/aclocal}" ] && mkdir -p "${FRIDA_PREFIX}/share/aclocal"
[ ! -d "${FRIDA_PREFIX}/lib}" ] && mkdir -p "${FRIDA_PREFIX}/lib"

if [ ! -d "${FRIDA_BUILD}/toolchain" ]; then
  echo "Downloading and deploying toolchain..."
  ${download_command} "http://frida-ire.googlecode.com/files/toolchain-${build_os}-${toolchain_version}.tar.bz2" | tar -C "${FRIDA_BUILD}" -xj ${tar_stdin} || exit 1
fi

if [ $FRIDA_ENVIRONMENT = normal ] && [ ! -d "${FRIDA_BUILD}/sdk-${FRIDA_TARGET}" ]; then
  echo "Downloading and deploying SDK for ${FRIDA_TARGET}..."
  ${download_command} "http://frida-ire.googlecode.com/files/sdk-${FRIDA_TARGET}-${sdk_version}.tar.bz2" | tar -C "${FRIDA_BUILD}" -xj ${tar_stdin} || exit 1
fi

for template in $(find ${FRIDA_TOOLROOT} ${FRIDA_SDKROOT} -name "*.frida.in"); do
  target=$(echo $template | sed 's,\.frida\.in$,,')
  cp -a "$template" "$target"
  sed -e "s,@FRIDA_TOOLROOT@,${FRIDA_TOOLROOT},g" -e "s,@FRIDA_SDKROOT@,${FRIDA_SDKROOT},g" "$template" > "$target"
done

(
  echo "export PKG_CONFIG=\"$PKG_CONFIG\""
  echo "export PKG_CONFIG_PATH=\"$PKG_CONFIG_PATH\""
  echo "export VALAC=\"$VALAC\""
  echo "export CC=\"$CC\""
  echo "export CFLAGS=\"$CFLAGS\""
  echo "export LD=\"$LD\""
  echo "export LDFLAGS=\"$LDFLAGS\""
  echo "export CXXFLAGS=\"$CXXFLAGS\""
  echo "export OBJCFLAGS=\"$OBJCFLAGS\""
  echo "export CPPFLAGS=\"$CPPFLAGS\""
) > frida-env.rc

echo "Environment created. To enter:"
echo "# source $(pwd)/frida-env.rc"
