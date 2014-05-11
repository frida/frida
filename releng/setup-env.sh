#!/bin/bash

releng_path=`dirname $0`

build_platform=$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$$,mac,')
build_arch=$(uname -m)
build_platform_arch=${build_platform}-${build_arch}

if [ -n "$FRIDA_HOST" ]; then
  host_platform=$(echo -n $FRIDA_HOST | sed 's,\([a-z]\+\)-\(.\+\),\1,g')
else
  host_platform=$build_platform
fi
if [ -n "$FRIDA_HOST" ]; then
  host_arch=$(echo -n $FRIDA_HOST | sed 's,\([a-z]\+\)-\(.\+\),\2,g')
else
  host_arch=$(uname -m)
fi
host_platform_arch=${host_platform}-${host_arch}

case $build_platform in
  linux)
    download_command="wget -O - -q"
    tar_stdin=""
    ;;
  mac)
    download_command="curl -sS"
    tar_stdin="-"
    ;;
  *)
    echo "Could not determine build platform" > /dev/stderr
    exit 1
esac

if [ -z "$FRIDA_HOST" ]; then
  echo "Assuming host is $host_platform_arch Set FRIDA_HOST to override."
fi

if [ $host_platform = "android" -a -z "$ANDROID_NDK_ROOT" ]; then
  echo "ANDROID_NDK_ROOT must be set" > /dev/stderr
  exit 1
fi

prompt_color=33

toolchain_version=20140511
sdk_version=20140511

if [ -n "$FRIDA_ENV_NAME" ]; then
  frida_env_name_prefix=${FRIDA_ENV_NAME}-
else
  frida_env_name_prefix=
fi

pushd $releng_path/../ > /dev/null
FRIDA_ROOT=`pwd`
popd > /dev/null
FRIDA_BUILD="$FRIDA_ROOT/build"
FRIDA_PREFIX="$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}"
FRIDA_PREFIX_LIB="$FRIDA_PREFIX/lib"
FRIDA_TOOLROOT="$FRIDA_BUILD/${frida_env_name_prefix}toolchain-${build_platform_arch}"
FRIDA_SDKROOT="$FRIDA_BUILD/${frida_env_name_prefix}sdk-${host_platform_arch}"

CFLAGS=""
CXXFLAGS=""
CPPFLAGS=""
LDFLAGS=""

case $host_platform in
  linux)
    CPP="/usr/bin/cpp"
    CC="/usr/bin/gcc"
    CXX="/usr/bin/g++"
    OBJC=""
    LD="/usr/bin/ld"
    AR="/usr/bin/ar"
    NM="/usr/bin/nm"
    OBJDUMP="/usr/bin/objdump"
    RANLIB="/usr/bin/ranlib"
    STRIP="/usr/bin/strip"

    CFLAGS="-ffunction-sections -fdata-sections"
    CPPFLAGS="-I$FRIDA_SDKROOT/include"
    LDFLAGS="-Wl,--gc-sections -L$FRIDA_SDKROOT/lib"
    ;;
  mac)
    CPP="/usr/bin/cpp"
    CC="/usr/bin/clang"
    CXX="/usr/bin/clang++"
    OBJC="/usr/bin/clang"
    LD="/usr/bin/clang"

    case $host_arch in
      i386)
        CFLAGS="-m32"
        ;;
      x86_64)
        CFLAGS="-m64"
        ;;
    esac
    LDFLAGS="-Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  ios)
    ios_sdkver="7.1"
    ios_sdk="iphoneos$ios_sdkver"
    ios_minver="7.0"

    CPP="$(xcrun --sdk $ios_sdk -f cpp)"
    CC="$(xcrun --sdk $ios_sdk -f clang)"
    CXX="$(xcrun --sdk $ios_sdk -f clang++)"
    OBJC="$(xcrun --sdk $ios_sdk -f clang)"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    ios_dev="$(dirname $(dirname $(dirname $(xcrun --sdk $ios_sdk -f iphoneos-optimize))))"
    ios_sdk="$ios_dev/SDKs/iPhoneOS$ios_sdkver.sdk"

    [ $host_arch == 'arm' ] && ios_arch=armv7 || ios_arch=arm64

    CFLAGS="-isysroot $ios_sdk -miphoneos-version-min=$ios_minver -arch $ios_arch"
    LDFLAGS="-isysroot $ios_sdk -Wl,-iphoneos_version_min,$ios_minver -arch $ios_arch -Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  android)
    android_clang_prefix="$ANDROID_NDK_ROOT/toolchains/llvm-3.4/prebuilt/darwin-x86_64"
    android_gcc_toolchain="$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.8/prebuilt/darwin-x86_64"
    android_sysroot="$ANDROID_NDK_ROOT/platforms/android-14/arch-arm"

    toolflags="--sysroot=$android_sysroot \
-gcc-toolchain $android_gcc_toolchain \
-target armv7-none-linux-androideabi \
-no-canonical-prefixes"
    CPP="$android_gcc_toolchain/bin/arm-linux-androideabi-cpp --sysroot=$android_sysroot"
    CC="$android_clang_prefix/bin/clang $toolflags"
    CXX="$android_clang_prefix/bin/clang++ $toolflags"
    LD="$android_gcc_toolchain/bin/arm-linux-androideabi-ld --sysroot=$android_sysroot"
    AR="$android_gcc_toolchain/bin/arm-linux-androideabi-ar"
    NM="$android_gcc_toolchain/bin/arm-linux-androideabi-nm"
    OBJDUMP="$android_gcc_toolchain/bin/arm-linux-androideabi-objdump"
    RANLIB="$android_gcc_toolchain/bin/arm-linux-androideabi-ranlib"
    STRIP="$android_gcc_toolchain/bin/arm-linux-androideabi-strip"

    CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 \
-ffunction-sections -funwind-tables -fno-exceptions -fno-rtti \
-DANDROID \
-I$android_sysroot/usr/include \
-I$FRIDA_SDKROOT/include"
    CXXFLAGS="\
-I$ANDROID_NDK_ROOT/sources/cxx-stl/llvm-libc++/libcxx/include \
-I$ANDROID_NDK_ROOT/sources/cxx-stl/gabi++/include \
-I$ANDROID_NDK_ROOT/sources/android/support/include"
    CPPFLAGS="-DANDROID \
-I$android_sysroot/usr/include \
-I$FRIDA_SDKROOT/include"
    LDFLAGS="-Wl,--fix-cortex-a8 \
-Wl,--no-undefined \
-Wl,-z,noexecstack \
-Wl,-z,relro \
-Wl,-z,now \
-L$ANDROID_NDK_ROOT/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a \
-L$FRIDA_SDKROOT/lib \
-lgcc -lc -lm"
    ;;
esac

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS"

ACLOCAL_FLAGS="-I $FRIDA_PREFIX/share/aclocal -I $FRIDA_SDKROOT/share/aclocal -I $FRIDA_TOOLROOT/share/aclocal"
ACLOCAL="aclocal $ACLOCAL_FLAGS"
CONFIG_SITE="$FRIDA_BUILD/${frida_env_name_prefix}config-${host_platform_arch}.site"

PKG_CONFIG="$FRIDA_TOOLROOT/bin/pkg-config --define-variable=frida_sdk_prefix=$FRIDA_SDKROOT --static"
PKG_CONFIG_PATH="$FRIDA_PREFIX_LIB/pkgconfig:$FRIDA_SDKROOT/lib/pkgconfig"

VALAC="$FRIDA_TOOLROOT/bin/valac-0.14 --vapidir=\"$FRIDA_TOOLROOT/share/vala-0.14/vapi\""
VALAC="$VALAC --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\" --vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

if [ ! -f "$FRIDA_TOOLROOT/.stamp" ]; then
  rm -rf "$FRIDA_TOOLROOT"
  mkdir -p "$FRIDA_TOOLROOT"
  echo "Downloading and deploying toolchain..."
  $download_command "http://build.frida.re/toolchain-${toolchain_version}-${build_platform}-${build_arch}.tar.bz2" | tar -C "$FRIDA_TOOLROOT" -xj $tar_stdin || exit 1
  touch "$FRIDA_TOOLROOT/.stamp"
fi

if [ ! -f "$FRIDA_SDKROOT/.stamp" ]; then
  rm -rf "$FRIDA_SDKROOT"
  mkdir -p "$FRIDA_SDKROOT"
  local_sdk=$FRIDA_BUILD/sdk-${host_platform}-${host_arch}.tar.bz2
  if [ -f $local_sdk ]; then
    echo "Deploying local SDK $(basename $local_sdk)..."
    tar -C "$FRIDA_SDKROOT" -xjf $local_sdk || exit 1
  else
    echo "Downloading and deploying SDK for ${host_platform_arch}..."
    $download_command "http://build.frida.re/sdk-${sdk_version}-${host_platform}-${host_arch}.tar.bz2" | tar -C "$FRIDA_SDKROOT" -xj $tar_stdin 2> /dev/null
    if [ $? -ne 0 ]; then
      echo ""
      echo "Bummer. It seems we don't have a prebuilt SDK for your system."
      echo ""
      echo "Please go ahead and build it yourself:"
      echo "$ make -f Makefile.sdk.mk"
      echo ""
      echo "Afterwards just retry and the SDK will get picked up automatically."
      echo ""
      exit 1
    fi
  fi
  touch "$FRIDA_SDKROOT/.stamp"
fi

for template in $(find $FRIDA_TOOLROOT $FRIDA_SDKROOT -name "*.frida.in"); do
  target=$(echo $template | sed 's,\.frida\.in$,,')
  cp -a "$template" "$target"
  sed \
    -e "s,@FRIDA_TOOLROOT@,$FRIDA_TOOLROOT,g" \
    -e "s,@FRIDA_SDKROOT@,$FRIDA_SDKROOT,g" \
    "$template" > "$target"
done

env_rc=build/${FRIDA_ENV_NAME:-frida}-env-${host_platform_arch}.rc

(
  echo "export PATH=\"$FRIDA_TOOLROOT/bin:\$PATH\""
  echo "export PS1=\"\e[0;${prompt_color}m[\u@\h \w \e[m\e[1;${prompt_color}mfrida-${host_platform_arch}\e[m\e[0;${prompt_color}m]\e[m\n\$ \""
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
) > $env_rc

case $host_platform in
  linux|android)
    (
      echo "export AR=\"$AR\""
      echo "export NM=\"$NM\""
      echo "export OBJDUMP=\"$OBJDUMP\""
      echo "export RANLIB=\"$RANLIB\""
      echo "export STRIP=\"$STRIP\""
    ) >> $env_rc
    ;;
  mac|ios)
    (
      echo "export OBJC=\"$OBJC\""
      echo "export OBJCFLAGS=\"$CFLAGS\""
      echo "export MACOSX_DEPLOYMENT_TARGET=10.7"
    ) >> $env_rc
    ;;
esac

sed \
  -e "s,@frida_host_platform@,$host_platform,g" \
  -e "s,@frida_host_arch@,$host_arch,g" \
  -e "s,@frida_host_platform_arch@,$host_platform_arch,g" \
  -e "s,@frida_prefix@,$FRIDA_PREFIX,g" \
  $releng_path/config.site.in > "$CONFIG_SITE"

echo "Environment created. To enter:"
echo "# source ${FRIDA_ROOT}/$env_rc"
