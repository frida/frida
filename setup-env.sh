#!/bin/bash

build_os=$(uname -s | tr '[A-Z]' '[a-z]')
prompt_color=33

case $build_os in
  linux)
    download_command="wget -O - -nv"
    tar_stdin=""
    ;;
  darwin)
    download_command="curl -sS"
    tar_stdin="-"

    build_os=mac
    ;;
  *)
    echo "Could not determine build OS"
    exit 1
esac

if [ -z $FRIDA_TARGET ] ; then
  if [ $build_os = 'linux' ] ; then
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
  else
    FRIDA_TARGET=mac64
  fi
  echo "Assuming target is $FRIDA_TARGET. Set FRIDA_TARGET to override."
fi

toolchain_version=20130406
case $FRIDA_TARGET in
  linux-*)
    sdk_version=20130212
    ;;
  mac32|mac64|ios)
    sdk_version=20130309
    ;;
esac

pushd `dirname $0` > /dev/null
FRIDA_ROOT=`pwd`
popd > /dev/null
FRIDA_BUILD="$FRIDA_ROOT/build"
FRIDA_PREFIX="$FRIDA_BUILD/frida-$FRIDA_TARGET"
FRIDA_PREFIX_LIB="$FRIDA_PREFIX/lib"
FRIDA_TOOLROOT="$FRIDA_BUILD/toolchain"
FRIDA_SDKROOT="$FRIDA_BUILD/sdk-$FRIDA_TARGET"

case $FRIDA_TARGET in
  linux-*)
    CPP="/usr/bin/cpp"
    CC="/usr/bin/gcc-4.6"
    CXX="/usr/bin/g++-4.6"
    OBJC=""
    LD="/usr/bin/ld"

    CFLAGS="-ffunction-sections -fdata-sections"
    LDFLAGS="-Wl,--gc-sections"
    ;;
  mac32|mac64)
    CPP="/usr/bin/cpp"
    CC="/usr/bin/clang"
    CXX="/usr/bin/clang++"
    OBJC="/usr/bin/clang"
    LD="/usr/bin/clang"

    case $FRIDA_TARGET in
      mac32)
        CFLAGS="-m32"
        ;;
      mac64)
        CFLAGS="-m64"
        ;;
    esac
    LDFLAGS="-no-undefined"
    ;;
  ios)
    ios_sdkver="6.1"
    ios_sdk="iphoneos$ios_sdkver"
    ios_minver="6.0"

    CPP="$(xcrun --sdk $ios_sdk -f cpp)"
    CC="$(xcrun --sdk $ios_sdk -f clang)"
    CXX="$(xcrun --sdk $ios_sdk -f clang++)"
    OBJC="$(xcrun --sdk $ios_sdk -f clang)"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    IOS_DEVROOT="$(dirname $(dirname $(dirname $(xcrun --sdk iphoneos6.1 -f iphoneos-optimize))))"
    IOS_SDKROOT="$IOS_DEVROOT/SDKs/iPhoneOS$ios_sdkver.sdk"

    CFLAGS="-isysroot $IOS_SDKROOT -miphoneos-version-min=$ios_minver -arch armv7"
    LDFLAGS="-isysroot $IOS_SDKROOT -Wl,-iphoneos_version_min,$ios_minver -arch armv7 -no-undefined"
    ;;
esac

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS"
CPPFLAGS=""

ACLOCAL_FLAGS="-I $FRIDA_PREFIX/share/aclocal -I $FRIDA_SDKROOT/share/aclocal -I $FRIDA_TOOLROOT/share/aclocal"
ACLOCAL="aclocal $ACLOCAL_FLAGS"
CONFIG_SITE="$FRIDA_BUILD/config-${FRIDA_TARGET}.site"

PKG_CONFIG="$FRIDA_TOOLROOT/bin/pkg-config --define-variable=frida_sdk_prefix=$FRIDA_SDKROOT --static"
PKG_CONFIG_PATH="$FRIDA_PREFIX_LIB/pkgconfig:$FRIDA_SDKROOT/lib/pkgconfig"

VALAC="$FRIDA_TOOLROOT/bin/valac-0.14 --vapidir=\"$FRIDA_TOOLROOT/share/vala-0.14/vapi\""
VALAC="$VALAC --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\" --vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

if [ ! -d "$FRIDA_BUILD/toolchain" ]; then
  echo "Downloading and deploying toolchain..."
  $download_command "http://ospy.org/toolchain-$build_os-$toolchain_version.tar.bz2" | tar -C "$FRIDA_BUILD" -xj $tar_stdin || exit 1
fi

if [ ! -d "$FRIDA_BUILD/sdk-$FRIDA_TARGET" ]; then
  echo "Downloading and deploying SDK for $FRIDA_TARGET..."
  $download_command "http://frida-ire.googlecode.com/files/sdk-$FRIDA_TARGET-$sdk_version.tar.bz2" | tar -C "$FRIDA_BUILD" -xj $tar_stdin || exit 1
fi

for template in $(find $FRIDA_TOOLROOT $FRIDA_SDKROOT -name "*.frida.in"); do
  target=$(echo $template | sed 's,\.frida\.in$,,')
  cp -a "$template" "$target"
  sed \
    -e "s,@FRIDA_TOOLROOT@,$FRIDA_TOOLROOT,g" \
    -e "s,@FRIDA_SDKROOT@,$FRIDA_SDKROOT,g" \
    "$template" > "$target"
done

(
  echo "export PATH=\"$FRIDA_TOOLROOT/bin:\$PATH\""
  echo "export PS1=\"\e[0;${prompt_color}m[\u@\h \w \e[m\e[1;${prompt_color}mfrida-$FRIDA_TARGET\e[m\e[0;${prompt_color}m]\e[m\n\$ \""
  echo "export PKG_CONFIG=\"$PKG_CONFIG\""
  echo "export PKG_CONFIG_PATH=\"$PKG_CONFIG_PATH\""
  echo "export VALAC=\"$VALAC\""
  echo "export CPP=\"$CPP\""
  echo "export CPPFLAGS=\"$CPPFLAGS\""
  echo "export CC=\"$CC\""
  echo "export CFLAGS=\"$CFLAGS\""
  echo "export CXX=\"$CXX\""
  echo "export CXXFLAGS=\"$CXXFLAGS\""
  echo "export LD=\"$LD\""
  echo "export LDFLAGS=\"$LDFLAGS\""
  echo "export ACLOCAL_FLAGS=\"$ACLOCAL_FLAGS\""
  echo "export ACLOCAL=\"$ACLOCAL\""
  echo "export CONFIG_SITE=\"$CONFIG_SITE\""
) > build/frida-env-${FRIDA_TARGET}.rc

case $FRIDA_TARGET in
  mac32|mac64|ios)
    (
      echo "export OBJC=\"$OBJC\""
      echo "export OBJCFLAGS=\"$CFLAGS\""
      echo "export MACOSX_DEPLOYMENT_TARGET=10.6"
    ) >> build/frida-env-${FRIDA_TARGET}.rc
    ;;
esac

sed \
  -e "s,@frida_target@,$FRIDA_TARGET,g" \
  -e "s,@frida_prefix@,$FRIDA_PREFIX,g" \
  config.site.in > $CONFIG_SITE

echo "Environment created. To enter:"
echo "# source $FRIDA_ROOT/build/frida-env-${FRIDA_TARGET}.rc"
