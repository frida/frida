#!/bin/bash

releng_path=`dirname $0`

build_platform=$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,mac,')
build_arch=$(uname -m)
build_platform_arch=${build_platform}-${build_arch}

if [ -n "$FRIDA_HOST" ]; then
  host_platform=$(echo -n $FRIDA_HOST | cut -f1 -d"-")
else
  host_platform=$build_platform
fi
if [ -n "$FRIDA_HOST" ]; then
  host_arch=$(echo -n $FRIDA_HOST | cut -f2 -d"-")
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

toolchain_version=20141117
case $host_platform in
  linux|android)
    sdk_version=20150222
    ;;
  *)
    sdk_version=20141117
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
    OBJC=""
    LD="/usr/bin/ld"
    AR="/usr/bin/ar"
    NM="/usr/bin/nm"
    OBJDUMP="/usr/bin/objdump"
    RANLIB="/usr/bin/ranlib"
    STRIP="/usr/bin/strip"

    [ $host_arch == 'i386' ] && host_arch_flags="-m32" || host_arch_flags="-m64"

    CFLAGS="$host_arch_flags -ffunction-sections -fdata-sections"
    LDFLAGS="$host_arch_flags -Wl,--no-undefined -Wl,--gc-sections"
    if [ "$FRIDA_ENV_SDK" != 'none' ]; then
      CFLAGS="$CFLAGS -I$FRIDA_SDKROOT/include"
      CPPFLAGS="-I$FRIDA_SDKROOT/include"
      LDFLAGS="$LDFLAGS -L$FRIDA_SDKROOT/lib"
    fi
    ;;
  qnx)
    qnx_toolchain_dir=$QNX_HOST/usr/bin
    qnx_host_target=$host_arch-unknown-nto-qnx6.6.0eabi
    qnx_toolchain_prefix=$qnx_toolchain_dir/$qnx_host_target
    qnx_libdir=$QNX_TARGET/$host_arch/armle/lib
    echo "using qnx toolchain with prefix: $qnx_toolchain_prefix"
    export PATH="$qnx_toolchain_dir:$PATH"
    CPP="$qnx_toolchain_prefix-cpp"
    CC="$qnx_toolchain_prefix-gcc -static-libgcc"
    CXX="$qnx_toolchain_prefix-g++ -static-libgcc"
    OBJC=""
    LD="$qnx_toolchain_prefix-ld -rpath=$qnx_libdir"
    # rm $qnx_toolchain_dir/ld
    # echo ln -s $LD $qnx_toolchain_dir/ld
    AR="$qnx_toolchain_prefix-ar"
    NM="$qnx_toolchain_prefix-nm"
    OBJDUMP="$qnx_toolchain_prefix-objdump"
    RANLIB="$qnx_toolchain_prefix-ranlib"
    STRIP="$qnx_toolchain_prefix-strip"
    CFLAGS="-D_QNX_SOURCE"
    ;;
  mac)
    mac_minver="10.7"
    mac_sdkver="10.10"

    mac_sdk="macosx$mac_sdkver"
    mac_sdk_path="$(xcrun --sdk $mac_sdk --show-sdk-path)"

    CPP="$(xcrun --sdk $mac_sdk -f cpp)"
    CC="$(xcrun --sdk $mac_sdk -f clang)"
    CXX="$(xcrun --sdk $mac_sdk -f clang++)"
    OBJC="$(xcrun --sdk $mac_sdk -f clang)"
    LD="$(xcrun --sdk $mac_sdk -f ld)"

    CFLAGS="-isysroot $mac_sdk_path -mmacosx-version-min=$mac_minver -arch $host_arch"
    CXXFLAGS="-stdlib=libc++"
    LDFLAGS="-isysroot $mac_sdk_path -Wl,-macosx_version_min,$mac_minver -arch $host_arch -Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  ios)
    ios_minver="7.0"
    ios_sdkver="8.2"

    ios_sdk="iphoneos$ios_sdkver"
    ios_sdk_path="$(xcrun --sdk $ios_sdk --show-sdk-path)"

    CPP="$(xcrun --sdk $ios_sdk -f cpp)"
    CC="$(xcrun --sdk $ios_sdk -f clang)"
    CXX="$(xcrun --sdk $ios_sdk -f clang++)"
    OBJC="$(xcrun --sdk $ios_sdk -f clang)"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    [ $host_arch == 'arm' ] && ios_arch=armv7 || ios_arch=arm64

    CFLAGS="-isysroot $ios_sdk_path -miphoneos-version-min=$ios_minver -arch $ios_arch"
    CXXFLAGS="-stdlib=libc++"
    LDFLAGS="-isysroot $ios_sdk_path -Wl,-iphoneos_version_min,$ios_minver -arch $ios_arch -Wl,-dead_strip -Wl,-no_compact_unwind"
    ;;
  android)
    android_build_platform=$(echo ${build_platform} | sed 's,^mac$,darwin,')
    android_host_arch=$(echo ${host_arch} | sed 's,^i386$,x86,')
    case $android_host_arch in
      x86)
        android_host_abi=x86
        android_host_target=i686-none-linux-android
        android_host_toolchain=x86-4.8
        android_host_toolprefix=i686-linux-android-
        android_host_cflags="-march=i686"
        android_host_ldflags=""
        ;;
      x86_64)
        android_host_abi=x86_64
        android_host_target=x86_64-none-linux-android
        android_host_toolchain=x86_64-4.9
        android_host_toolprefix=x86_64-linux-android-
        android_host_cflags="-march=x86_64"
        android_host_ldflags=""
        ;;
      arm)
        android_host_abi=armeabi-v7a
        android_host_target=armv7-none-linux-androideabi
        android_host_toolchain=arm-linux-androideabi-4.8
        android_host_toolprefix=arm-linux-androideabi-
        android_host_cflags="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
        android_host_ldflags="-Wl,--fix-cortex-a8"
        ;;
      arm64)
        android_host_abi=arm64-v8a
        android_host_target=aarch64-none-linux-android
        android_host_toolchain=aarch64-linux-android-4.9
        android_host_toolprefix=aarch64-linux-android-
        android_host_cflags="-march=arm64"
        android_host_ldflags=""
        ;;
    esac

    android_clang_prefix="$ANDROID_NDK_ROOT/toolchains/llvm-3.4/prebuilt/${android_build_platform}-x86_64"
    android_gcc_toolchain="$ANDROID_NDK_ROOT/toolchains/${android_host_toolchain}/prebuilt/${android_build_platform}-x86_64"
    android_sysroot="$ANDROID_NDK_ROOT/platforms/android-14/arch-${android_host_arch}"

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
    OBJDUMP="$android_gcc_toolchain/bin/${android_host_toolprefix}objdump"
    RANLIB="$android_gcc_toolchain/bin/${android_host_toolprefix}ranlib"
    STRIP="$android_gcc_toolchain/bin/${android_host_toolprefix}strip"

    CFLAGS="$android_host_cflags \
-ffunction-sections -funwind-tables -fno-exceptions -fno-rtti \
-DANDROID \
-I$android_sysroot/usr/include"
    CXXFLAGS="\
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
esac

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS"

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

VALAC="$FRIDA_TOOLROOT/bin/valac-0.26 --vapidir=\"$FRIDA_TOOLROOT/share/vala-0.26/vapi\""
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  VALAC="$VALAC --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\""
fi
VALAC="$VALAC --vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

if [ ! -f "$FRIDA_TOOLROOT/.stamp" ]; then
  rm -rf "$FRIDA_TOOLROOT"
  mkdir -p "$FRIDA_TOOLROOT"

  echo "Downloading and deploying toolchain..."
  $download_command "https://build.frida.re/toolchain-${toolchain_version}-${build_platform}-${build_arch}.tar.bz2" | tar -C "$FRIDA_TOOLROOT" -xj $tar_stdin || exit 1

  for template in $(find $FRIDA_TOOLROOT -name "*.frida.in"); do
    target=$(echo $template | sed 's,\.frida\.in$,,')
    cp -a "$template" "$target"
    sed \
      -e "s,@FRIDA_TOOLROOT@,$FRIDA_TOOLROOT,g" \
      "$template" > "$target"
  done

  touch "$FRIDA_TOOLROOT/.stamp"
fi

if [ "$FRIDA_ENV_SDK" != 'none' -a ! -f "$FRIDA_SDKROOT/.stamp" ]; then
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

  touch "$FRIDA_SDKROOT/.stamp"
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
