#!/bin/bash

releng_path=`dirname $0`

build_platform=$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,mac,')
build_arch=$($releng_path/detect-arch.sh)
build_platform_arch=${build_platform}-${build_arch}

if [ -n "$FRIDA_HOST" ]; then
  host_platform=$(echo -n $FRIDA_HOST | cut -f1 -d"-")
else
  host_platform=$build_platform
fi
if [ -n "$FRIDA_HOST" ]; then
  host_arch=$(echo -n $FRIDA_HOST | cut -f2 -d"-")
else
  host_arch=$build_arch
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

if [ $host_platform = "android" ]; then
  ndk_required=r10d
  if [ -n "$ANDROID_NDK_ROOT" ]; then
    ndk_installed=$(cut -f1 -d" " "$ANDROID_NDK_ROOT/RELEASE.TXT")
    if [ "$ndk_installed" != "$ndk_required" ]; then
      echo "Unsupported NDK version: ${ndk_installed}. Please install ${ndk_required}."               > /dev/stderr
      echo ""                                                                                         > /dev/stderr
      echo "Frida's SDK - the prebuilt dependencies snapshot - was compiled against ${ndk_required}," > /dev/stderr
      echo "and as we have observed the NDK ABI breaking over time, we ask you to install"            > /dev/stderr
      echo "the exact same version."                                                                  > /dev/stderr
      echo ""                                                                                         > /dev/stderr
      echo "However, if you'd like to take the risk and use a different NDK, you may edit"            > /dev/stderr
      echo "releng/setup-env.sh and adjust the ndk_required variable. Make sure you use"              > /dev/stderr
      echo "a newer NDK, and not an older one. Note that the proper solution is to rebuild"           > /dev/stderr
      echo "the SDK against your NDK by running:"                                                     > /dev/stderr
      echo "  make -f Makefile.sdk.mk FRIDA_HOST=android-arm"                                         > /dev/stderr
      echo "If you do this and it works well for you, please let us know so we can upgrade"           > /dev/stderr
      echo "the upstream SDK version."                                                                > /dev/stderr
      exit 1
    fi
  else
    echo "ANDROID_NDK_ROOT must be set to the location of your $frida_ndk NDK." > /dev/stderr
    exit 1
  fi
fi

if [ $host_platform = "qnx" ]; then
  if [ ! -n "$QNX_HOST" ]; then
    echo "You need to specify QNX_HOST and QNX_TARGET"
    exit 1
  fi
fi

prompt_color=33

toolchain_version=20150406
case $host_platform_arch in
  ios-arm64|android-arm|android-arm64)
    sdk_version=20150512
    ;;
  *)
    sdk_version=20150423
    ;;
esac

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
    CC="/usr/bin/gcc -static-libgcc -static-libstdc++"
    CXX="/usr/bin/g++ -static-libgcc -static-libstdc++"
    LD="/usr/bin/ld"

    AR="/usr/bin/ar"
    NM="/usr/bin/nm"
    RANLIB="/usr/bin/ranlib"
    STRIP="/usr/bin/strip"

    OBJDUMP="/usr/bin/objdump"

    [ $host_arch == 'i386' ] && host_arch_flags="-m32" || host_arch_flags="-m64"

    CFLAGS="$host_arch_flags -ffunction-sections -fdata-sections"
    CXXFLAGS="-std=c++11"
    LDFLAGS="$host_arch_flags -Wl,--no-undefined -Wl,--gc-sections"
    if [ "$FRIDA_ENV_SDK" != 'none' ]; then
      CFLAGS="$CFLAGS -I$FRIDA_SDKROOT/include"
      CPPFLAGS="-I$FRIDA_SDKROOT/include"
      LDFLAGS="$LDFLAGS -L$FRIDA_SDKROOT/lib"
    fi
    ;;
  mac)
    mac_minver="10.7"
    mac_sdkver="10.10"

    mac_sdk="macosx$mac_sdkver"
    mac_sdk_path="$(xcrun --sdk $mac_sdk --show-sdk-path)"

    CPP="$(xcrun --sdk $mac_sdk -f clang) -E"
    CC="$(xcrun --sdk $mac_sdk -f clang)"
    CXX="$(xcrun --sdk $mac_sdk -f clang++)"
    OBJC="$(xcrun --sdk $mac_sdk -f clang)"
    LD="$(xcrun --sdk $mac_sdk -f ld)"

    AR="$(xcrun --sdk $mac_sdk -f ar)"
    NM="$(xcrun --sdk $mac_sdk -f nm)"
    RANLIB="$(xcrun --sdk $mac_sdk -f ranlib)"
    STRIP="$(xcrun --sdk $mac_sdk -f strip)"

    OTOOL="$(xcrun --sdk $mac_sdk -f otool)"
    CODESIGN="$(xcrun --sdk $mac_sdk -f codesign)"
    LIPO="$(xcrun --sdk $mac_sdk -f lipo)"

    CPPFLAGS="-isysroot $mac_sdk_path -mmacosx-version-min=$mac_minver -arch $host_arch"
    CFLAGS="-isysroot $mac_sdk_path -mmacosx-version-min=$mac_minver -arch $host_arch"
    CXXFLAGS="-std=c++11 -stdlib=libc++"
    LDFLAGS="-isysroot $mac_sdk_path -Wl,-macosx_version_min,$mac_minver -arch $host_arch -Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  ios)
    ios_minver="7.0"
    ios_sdkver="8.3"

    ios_sdk="iphoneos$ios_sdkver"
    ios_sdk_path="$(xcrun --sdk $ios_sdk --show-sdk-path)"

    CPP="$(xcrun --sdk $ios_sdk -f clang) -E"
    CC="$(xcrun --sdk $ios_sdk -f clang)"
    CXX="$(xcrun --sdk $ios_sdk -f clang++)"
    OBJC="$(xcrun --sdk $ios_sdk -f clang)"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    AR="$(xcrun --sdk $ios_sdk -f ar)"
    NM="$(xcrun --sdk $ios_sdk -f nm)"
    RANLIB="$(xcrun --sdk $ios_sdk -f ranlib)"
    STRIP="$(xcrun --sdk $ios_sdk -f strip)"

    OTOOL="$(xcrun --sdk $ios_sdk -f otool)"
    CODESIGN="$(xcrun --sdk $ios_sdk -f codesign)"
    LIPO="$(xcrun --sdk $ios_sdk -f lipo)"

    [ $host_arch == 'arm' ] && ios_arch=armv7 || ios_arch=arm64

    CPPFLAGS="-isysroot $ios_sdk_path -miphoneos-version-min=$ios_minver -arch $ios_arch"
    CFLAGS="-isysroot $ios_sdk_path -miphoneos-version-min=$ios_minver -arch $ios_arch"
    CXXFLAGS="-std=c++11 -stdlib=libc++"
    LDFLAGS="-isysroot $ios_sdk_path -Wl,-iphoneos_version_min,$ios_minver -arch $ios_arch -Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  android)
    android_build_platform=$(echo ${build_platform} | sed 's,^mac$,darwin,')
    android_host_arch=$(echo ${host_arch} | sed 's,^i386$,x86,')
    case $android_host_arch in
      x86)
        android_target_platform=14
        android_host_abi=x86
        android_host_target=i686-none-linux-android
        android_host_toolchain=x86-4.8
        android_host_toolprefix=i686-linux-android-
        android_host_cflags="-march=i686"
        android_host_ldflags=""
        ;;
      x86_64)
        android_target_platform=21
        android_host_abi=x86_64
        android_host_target=x86_64-none-linux-android
        android_host_toolchain=x86_64-4.9
        android_host_toolprefix=x86_64-linux-android-
        android_host_cflags="-march=x86_64"
        android_host_ldflags=""
        ;;
      arm)
        android_target_platform=14
        android_host_abi=armeabi-v7a
        android_host_target=armv7-none-linux-androideabi
        android_host_toolchain=arm-linux-androideabi-4.8
        android_host_toolprefix=arm-linux-androideabi-
        android_host_cflags="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
        android_host_ldflags="-Wl,--fix-cortex-a8"
        ;;
      arm64)
        android_target_platform=21
        android_host_abi=arm64-v8a
        android_host_target=aarch64-none-linux-android
        android_host_toolchain=aarch64-linux-android-4.9
        android_host_toolprefix=aarch64-linux-android-
        android_host_cflags=""
        android_host_ldflags=""
        ;;
    esac

    android_clang_prefix="$ANDROID_NDK_ROOT/toolchains/llvm-3.5/prebuilt/${android_build_platform}-x86_64"
    android_gcc_toolchain="$ANDROID_NDK_ROOT/toolchains/${android_host_toolchain}/prebuilt/${android_build_platform}-x86_64"
    android_sysroot="$ANDROID_NDK_ROOT/platforms/android-${android_target_platform}/arch-${android_host_arch}"
    toolflags="--sysroot=$android_sysroot \
--gcc-toolchain=$android_gcc_toolchain \
--target=$android_host_target \
-no-canonical-prefixes"

    CPP="$android_gcc_toolchain/bin/${android_host_toolprefix}cpp --sysroot=$android_sysroot"
    CC="$android_clang_prefix/bin/clang $toolflags"
    CXX="$android_clang_prefix/bin/clang++ $toolflags"
    LD="$android_gcc_toolchain/bin/${android_host_toolprefix}ld --sysroot=$android_sysroot"

    AR="$android_gcc_toolchain/bin/${android_host_toolprefix}ar"
    NM="$android_gcc_toolchain/bin/${android_host_toolprefix}nm"
    RANLIB="$android_gcc_toolchain/bin/${android_host_toolprefix}ranlib"
    STRIP="$android_gcc_toolchain/bin/${android_host_toolprefix}strip"

    OBJDUMP="$android_gcc_toolchain/bin/${android_host_toolprefix}objdump"

    CFLAGS="$android_host_cflags \
-ffunction-sections -funwind-tables -fno-exceptions -fno-rtti \
-DANDROID \
-I$android_sysroot/usr/include"
    CXXFLAGS="\
-std=c++11 \
-I$ANDROID_NDK_ROOT/sources/cxx-stl/llvm-libc++/libcxx/include \
-I$ANDROID_NDK_ROOT/sources/cxx-stl/gabi++/include \
-I$ANDROID_NDK_ROOT/sources/android/support/include"
    CPPFLAGS="-DANDROID \
-I$android_sysroot/usr/include"
    LDFLAGS="$android_host_ldflags \
-Wl,--no-undefined \
-Wl,--gc-sections \
-Wl,-z,noexecstack \
-Wl,-z,relro \
-Wl,-z,now \
-L$ANDROID_NDK_ROOT/sources/cxx-stl/llvm-libc++/libs/$android_host_abi \
-lm \
-lc++_static"
    if [ "$FRIDA_ENV_SDK" != 'none' ]; then
      CFLAGS="$CFLAGS -I$FRIDA_SDKROOT/include"
      CPPFLAGS="$CPPFLAGS -I$FRIDA_SDKROOT/include"
      LDFLAGS="$LDFLAGS -L$FRIDA_SDKROOT/lib"
    fi
    ;;
  qnx)
    case $host_arch in
      i386)
        qnx_host=i486-pc-nto-qnx6.6.0
        qnx_sysroot=$QNX_TARGET/x86
        ;;
      arm)
        qnx_host=arm-unknown-nto-qnx6.5.0
        qnx_sysroot=$QNX_TARGET/armle
        ;;
      *)
        echo "Unsupported QNX architecture" > /dev/stderr
        exit 1
        ;;
    esac

    qnx_toolchain_dir=$QNX_HOST/usr/bin
    qnx_toolchain_prefix=$qnx_toolchain_dir/$qnx_host
    qnx_preprocessor_flags="-include $FRIDA_ROOT/releng/frida-qnx-defines.h"

    PATH="$qnx_toolchain_dir:$PATH"

    CPP="$qnx_toolchain_prefix-cpp -march=armv6 --sysroot=$qnx_sysroot $qnx_preprocessor_flags"
    CC="$qnx_toolchain_prefix-gcc -march=armv6 --sysroot=$qnx_sysroot $qnx_preprocessor_flags -static-libgcc"
    CXX="$qnx_toolchain_prefix-g++ -march=armv6 --sysroot=$qnx_sysroot $qnx_preprocessor_flags -static-libgcc -static-libstdc++ -std=c++11"
    LD="$qnx_toolchain_prefix-ld -march=armv6 --sysroot=$qnx_sysroot"

    AR="$qnx_toolchain_prefix-ar"
    NM="$qnx_toolchain_prefix-nm"
    RANLIB="$qnx_toolchain_prefix-ranlib"
    STRIP="$qnx_toolchain_prefix-strip"

    OBJDUMP="$qnx_toolchain_prefix-objdump"

    CFLAGS="-ffunction-sections -fdata-sections"
    LDFLAGS="-Wl,--no-undefined -Wl,--gc-sections -L$(dirname $qnx_sysroot/lib/gcc/4.8.3/libstdc++.a)"

    if [ "$FRIDA_ENV_SDK" != 'none' ]; then
      CFLAGS="$CFLAGS -I$FRIDA_SDKROOT/include"
      CPPFLAGS="-I$FRIDA_SDKROOT/include"
      LDFLAGS="$LDFLAGS -L$FRIDA_SDKROOT/lib"
    else
      LDFLAGS="$LDFLAGS -L$FRIDA_PREFIX/lib"
    fi
    ;;
esac

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS"

if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  version_include="-include $FRIDA_BUILD/frida-version.h"
  CPPFLAGS="$version_include $CPPFLAGS"
  CFLAGS="$version_include $CFLAGS"
fi

ACLOCAL_FLAGS="-I $FRIDA_PREFIX/share/aclocal"
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $FRIDA_SDKROOT/share/aclocal"
fi
ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $FRIDA_TOOLROOT/share/aclocal"
ACLOCAL="aclocal $ACLOCAL_FLAGS"
CONFIG_SITE="$FRIDA_BUILD/${frida_env_name_prefix}config-${host_platform_arch}.site"

PKG_CONFIG="$FRIDA_TOOLROOT/bin/pkg-config --static"
PKG_CONFIG_PATH="$FRIDA_PREFIX_LIB/pkgconfig"
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  PKG_CONFIG="$PKG_CONFIG --define-variable=frida_sdk_prefix=$FRIDA_SDKROOT"
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$FRIDA_SDKROOT/lib/pkgconfig"
fi

VALAC="$FRIDA_TOOLROOT/bin/valac-0.28 --vapidir=\"$FRIDA_TOOLROOT/share/vala-0.28/vapi\""
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  VALAC="$VALAC --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\""
fi
VALAC="$VALAC --vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

if ! grep -Eq "^$toolchain_version\$" "$FRIDA_TOOLROOT/.version" 2>/dev/null; then
  rm -rf "$FRIDA_TOOLROOT"
  mkdir -p "$FRIDA_TOOLROOT"

  local_toolchain=$FRIDA_BUILD/toolchain-${build_platform}-${build_arch}.tar.bz2
  if [ -f $local_toolchain ]; then
    echo "Deploying local toolchain $(basename $local_toolchain)..."
    tar -C "$FRIDA_TOOLROOT" -xjf $local_toolchain || exit 1
  else
    echo "Downloading and deploying toolchain..."
    $download_command "https://build.frida.re/toolchain-${toolchain_version}-${build_platform}-${build_arch}.tar.bz2" | tar -C "$FRIDA_TOOLROOT" -xj $tar_stdin || exit 1
  fi

  for template in $(find $FRIDA_TOOLROOT -name "*.frida.in"); do
    target=$(echo $template | sed 's,\.frida\.in$,,')
    cp -a "$template" "$target"
    sed \
      -e "s,@FRIDA_TOOLROOT@,$FRIDA_TOOLROOT,g" \
      "$template" > "$target"
  done

  echo $toolchain_version > "$FRIDA_TOOLROOT/.version"
fi

if [ "$FRIDA_ENV_SDK" != 'none' ] && ! grep -Eq "^$sdk_version\$" "$FRIDA_SDKROOT/.version" 2>/dev/null; then
  rm -rf "$FRIDA_SDKROOT"
  mkdir -p "$FRIDA_SDKROOT"

  local_sdk=$FRIDA_BUILD/sdk-${host_platform}-${host_arch}.tar.bz2
  if [ -f $local_sdk ]; then
    echo "Deploying local SDK $(basename $local_sdk)..."
    tar -C "$FRIDA_SDKROOT" -xjf $local_sdk || exit 1
  else
    echo "Downloading and deploying SDK for ${host_platform_arch}..."
    $download_command "https://build.frida.re/sdk-${sdk_version}-${host_platform}-${host_arch}.tar.bz2" | tar -C "$FRIDA_SDKROOT" -xj $tar_stdin 2> /dev/null
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

  for template in $(find $FRIDA_SDKROOT -name "*.frida.in"); do
    target=$(echo $template | sed 's,\.frida\.in$,,')
    cp -a "$template" "$target"
    sed \
      -e "s,@FRIDA_SDKROOT@,$FRIDA_SDKROOT,g" \
      "$template" > "$target"
  done

  # TODO: fix this in libffi
  for name in libffi.pc gobject-2.0.pc; do
    pc=$FRIDA_SDKROOT/lib/pkgconfig/$name
    if grep -q '$(libdir)' $pc; then
      sed -e "s,\$(libdir),\${libdir},g" $pc > $pc.tmp
      cat $pc.tmp > $pc
      rm $pc.tmp
    fi
  done

  echo $sdk_version > "$FRIDA_SDKROOT/.version"
fi

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
  echo "export AR=\"$AR\""
  echo "export NM=\"$NM\""
  echo "export RANLIB=\"$RANLIB\""
  echo "export STRIP=\"$STRIP\""
  echo "export ACLOCAL_FLAGS=\"$ACLOCAL_FLAGS\""
  echo "export ACLOCAL=\"$ACLOCAL\""
  echo "export CONFIG_SITE=\"$CONFIG_SITE\""
  echo "unset LANG LC_COLLATE LC_CTYPE LC_MESSAGES LC_NUMERIC LC_TIME"
) > $env_rc

case $host_platform in
  linux|android|qnx)
    (
      echo "export OBJDUMP=\"$OBJDUMP\""
    ) >> $env_rc
    ;;
  mac|ios)
    (
      echo "export OTOOL=\"$OTOOL\""
      echo "export CODESIGN=\"$CODESIGN\""
      echo "export LIPO=\"$LIPO\""
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
