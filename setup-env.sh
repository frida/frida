#!/bin/bash

build_os=$(uname -s | tr '[A-Z]' '[a-z]')
[ "$build_os" = 'darwin' ] && build_os=mac
prompt_color=33

case $build_os in
  linux)
    download_command="wget -O - -nv"
    tar_stdin=""
    ;;
  mac)
    download_command="curl -sS"
    tar_stdin="-"
    ;;
  *)
    echo "Could not determine build OS" > /dev/stderr
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

case $FRIDA_TARGET in
  android)
    if [ -z "$ANDROID_NDK_ROOT" ]; then
      echo "ANDROID_NDK_ROOT must be set" > /dev/stderr
      exit 1
    fi
    ;;
esac

case $build_os in
  linux)
    toolchain_version=20130508
    ;;
  mac)
    toolchain_version=20131231
    ;;
esac

case $FRIDA_TARGET in
  linux-*)
    sdk_version=20130508
    ;;
  android)
    sdk_version=20131027
    ;;
  mac32|mac64)
    sdk_version=20131224
    ;;
  ios)
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

CFLAGS=""
CXXFLAGS=""
CPPFLAGS=""
LDFLAGS=""

case $FRIDA_TARGET in
  linux-*)
    CPP="/usr/bin/cpp"
    CC="/usr/bin/gcc"
    CXX="/usr/bin/g++"
    OBJC=""
    LD="/usr/bin/ld"

    CFLAGS="-ffunction-sections -fdata-sections"
    CPPFLAGS="-I$FRIDA_SDKROOT/include"
    LDFLAGS="-Wl,--gc-sections -L$FRIDA_SDKROOT/lib -lz"
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
    LDFLAGS="-no-undefined -Wl,-no_compact_unwind"
    ;;
  ios)
    ios_sdkver="7.0"
    ios_sdk="iphoneos$ios_sdkver"
    ios_minver="6.0"

    CPP="$(xcrun --sdk $ios_sdk -f cpp)"
    CC="$(xcrun --sdk $ios_sdk -f clang)"
    CXX="$(xcrun --sdk $ios_sdk -f clang++)"
    OBJC="$(xcrun --sdk $ios_sdk -f clang)"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    ios_dev="$(dirname $(dirname $(dirname $(xcrun --sdk $ios_sdk -f iphoneos-optimize))))"
    ios_sdk="$ios_dev/SDKs/iPhoneOS$ios_sdkver.sdk"

    CFLAGS="-isysroot $ios_sdk -miphoneos-version-min=$ios_minver -arch armv7"
    LDFLAGS="-isysroot $ios_sdk -Wl,-iphoneos_version_min,$ios_minver -arch armv7 -no-undefined -Wl,-no_compact_unwind"
    ;;
  android)
    android_clang_prefix="$ANDROID_NDK_ROOT/toolchains/llvm-3.3/prebuilt/darwin-x86_64"
    android_gcc_toolchain="$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.8/prebuilt/darwin-x86_64"
    android_sysroot="$ANDROID_NDK_ROOT/platforms/android-14/arch-arm"

    CPP="$android_gcc_toolchain/bin/arm-linux-androideabi-cpp"
    CC="$android_clang_prefix/bin/clang"
    CXX="$android_clang_prefix/bin/clang++"
    LD="$android_clang_prefix/bin/clang"
    AR="$android_gcc_toolchain/bin/arm-linux-androideabi-ar"
    OBJDUMP="$android_gcc_toolchain/bin/arm-linux-androideabi-objdump"
    RANLIB="$android_gcc_toolchain/bin/arm-linux-androideabi-ranlib"
    STRIP="$android_gcc_toolchain/bin/arm-linux-androideabi-strip"

    CFLAGS="--sysroot=$android_sysroot \
-gcc-toolchain $android_gcc_toolchain \
-target armv7-none-linux-androideabi \
-no-canonical-prefixes \
-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 \
-ffunction-sections -funwind-tables -fno-exceptions -fno-rtti \
-DANDROID -DUSE_STLPORT=1 -D_STLP_USE_PTR_SPECIALIZATIONS=1 \
-I$android_sysroot/usr/include \
-I$ANDROID_NDK_ROOT/sources/cxx-stl/stlport/stlport"
    CPPFLAGS="--sysroot=$android_sysroot \
-DANDROID -DUSE_STLPORT=1 -D_STLP_USE_PTR_SPECIALIZATIONS=1 \
-I$android_sysroot/usr/include \
-I$ANDROID_NDK_ROOT/sources/cxx-stl/stlport/stlport \
-I$FRIDA_SDKROOT/include"
    LDFLAGS="--sysroot=$android_sysroot \
-gcc-toolchain $android_gcc_toolchain \
-target armv7-none-linux-androideabi \
-no-canonical-prefixes \
-Wl,--fix-cortex-a8 \
-Wl,--no-undefined \
-Wl,-z,noexecstack \
-Wl,-z,relro \
-Wl,-z,now \
-L$android_sysroot/usr/lib \
-L$ANDROID_NDK_ROOT/sources/cxx-stl/stlport/libs/armeabi-v7a \
-L$FRIDA_SDKROOT/lib \
-lgcc -lc -lm -lstlport_static -lz"
    ;;
esac

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS"

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
  $download_command "http://ospy.org/sdk-$FRIDA_TARGET-$sdk_version.tar.bz2" | tar -C "$FRIDA_BUILD" -xj $tar_stdin || exit 1
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
  echo "unset LANG LC_COLLATE LC_CTYPE LC_MESSAGES LC_NUMERIC LC_TIME"
) > build/frida-env-${FRIDA_TARGET}.rc

case $FRIDA_TARGET in
  android)
    (
      echo "export AR=\"$AR\""
      echo "export OBJDUMP=\"$OBJDUMP\""
      echo "export RANLIB=\"$RANLIB\""
      echo "export STRIP=\"$STRIP\""
    ) >> build/frida-env-${FRIDA_TARGET}.rc
    ;;
  mac32|mac64|ios)
    (
      echo "export OBJC=\"$OBJC\""
      echo "export OBJCFLAGS=\"$CFLAGS\""
      echo "export MACOSX_DEPLOYMENT_TARGET=10.7"
    ) >> build/frida-env-${FRIDA_TARGET}.rc
    ;;
esac

sed \
  -e "s,@frida_target@,$FRIDA_TARGET,g" \
  -e "s,@frida_prefix@,$FRIDA_PREFIX,g" \
  config.site.in > $CONFIG_SITE

echo "Environment created. To enter:"
echo "# source $FRIDA_ROOT/build/frida-env-${FRIDA_TARGET}.rc"
