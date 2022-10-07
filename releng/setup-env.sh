#!/usr/bin/env bash

releng_path=`dirname $0`

build_os=$($releng_path/detect-os.sh)
build_arch=$($releng_path/detect-arch.sh)
build_os_arch=${build_os}-${build_arch}

if [ -n "$FRIDA_HOST" ]; then
  host_os=$(echo -n $FRIDA_HOST | cut -f1 -d"-")
else
  host_os=$build_os
fi
if [ -n "$FRIDA_HOST" ]; then
  host_arch=$(echo -n $FRIDA_HOST | cut -f2 -d"-")
else
  host_arch=$build_arch
fi
if [ -n "$FRIDA_LIBC" ]; then
  frida_libc=$FRIDA_LIBC
else
  case $host_arch in
    arm|armbe8)
      frida_libc=gnueabi
      ;;
    armhf)
      frida_libc=gnueabihf
      ;;
    mips64*)
      frida_libc=gnuabi64
      ;;
    *)
      frida_libc=gnu
      ;;
  esac
fi
case $host_arch in
  x86)
    host_clang_arch=i386
    ;;
  arm64eoabi)
    host_clang_arch=arm64e
    ;;
  *)
    host_clang_arch=$host_arch
    ;;
esac
host_os_arch=${host_os}-${host_arch}

read_toolchain_variable ()
{
  local var_name=$1
  local fallback=$2

  if [ $host_os_arch == $build_os_arch ] && [ "$FRIDA_CROSS" == yes ]; then
    local contextual_var_name=${var_name}_FOR_BUILD
  else
    local contextual_var_name=${var_name}
  fi

  echo ${!contextual_var_name:-$fallback}
}

case $host_os in
  macos|ios)
    meson_host_system=darwin
    ;;
  *)
    meson_host_system=$host_os
    ;;
esac
case $host_arch in
  i?86)
    meson_host_cpu_family=x86
    meson_host_cpu=i686
    meson_host_endian=little
    ;;
  arm)
    meson_host_cpu_family=arm
    meson_host_cpu=armv7
    meson_host_endian=little
    ;;
  armbe8)
    meson_host_cpu_family=arm
    meson_host_cpu=armv6
    meson_host_endian=big
    ;;
  armeabi)
    meson_host_cpu_family=arm
    meson_host_cpu=armv7eabi
    meson_host_endian=little
    ;;
  armhf)
    meson_host_cpu_family=arm
    meson_host_cpu=armv7hf
    meson_host_endian=little
    ;;
  arm64|arm64e|arm64eoabi)
    meson_host_cpu_family=aarch64
    meson_host_cpu=aarch64
    meson_host_endian=little
    ;;
  mips)
    meson_host_cpu_family=mips
    meson_host_cpu=mips
    meson_host_endian=big
    ;;
  mipsel)
    meson_host_cpu_family=mips
    meson_host_cpu=mips
    meson_host_endian=little
    ;;
  mips64)
    meson_host_cpu_family=mips64
    meson_host_cpu=mips64
    meson_host_endian=big
    ;;
  mips64el)
    meson_host_cpu_family=mips64
    meson_host_cpu=mips64
    meson_host_endian=little
    ;;
  s390x)
    meson_host_cpu_family=s390x
    meson_host_cpu=s390x
    meson_host_endian=big
    ;;
  *)
    meson_host_cpu_family=$host_arch
    meson_host_cpu=$host_arch
    meson_host_endian=little
    ;;
esac
meson_b_lundef=true

case $FRIDA_ASAN in
  yes|no)
    enable_asan=$FRIDA_ASAN
    ;;
  *)
    enable_asan=no
    ;;
esac

if which curl &>/dev/null; then
  download_command="curl --progress-bar"
elif which wget &>/dev/null; then
  download_command="wget -O - -q"
else
  echo "Please install curl or wget: required for downloading SDK and toolchain." > /dev/stderr
  exit 1
fi

if [ -z "$FRIDA_HOST" ]; then
  echo "Assuming host is $host_os_arch Set FRIDA_HOST to override."
fi

if [ "$host_os" == "android" ]; then
  ndk_required=25
  if [ -n "$ANDROID_NDK_ROOT" ]; then
    if [ -f "$ANDROID_NDK_ROOT/source.properties" ]; then
      ndk_installed_version=$(grep Pkg.Revision "$ANDROID_NDK_ROOT/source.properties" | awk '{ split($NF, v, "."); print v[1]; }')
    else
      ndk_installed_version=$(cut -f1 -d" " "$ANDROID_NDK_ROOT/RELEASE.TXT")
    fi
    if [ "$ndk_installed_version" -ne "$ndk_required" ]; then
      (
        echo ""
        echo "Unsupported NDK version $ndk_installed_version. Please install NDK r$ndk_required."
        echo ""
        echo "Frida's SDK - the prebuilt dependencies snapshot - was compiled against r$ndk_required,"
        echo "and as we have observed the NDK ABI breaking over time, we ask that you install"
        echo "the exact same version."
        echo ""
        echo "However, if you'd like to take the risk and use a different NDK, you may edit"
        echo "releng/setup-env.sh and adjust the ndk_required variable. Make sure you use"
        echo "a newer NDK, and not an older one. Note that the proper solution is to rebuild"
        echo "the SDK against your NDK by running:"
        echo "  make -f Makefile.sdk.mk FRIDA_HOST=android-arm"
        echo "If you do this and it works well for you, please let us know so we can upgrade"
        echo "the upstream SDK version."
        echo ""
      ) > /dev/stderr
      exit 1
    fi
  else
    echo "ANDROID_NDK_ROOT must be set to the location of your r$ndk_required NDK." > /dev/stderr
    exit 1
  fi
fi

if [ "$host_os" == "qnx" ]; then
  if [ ! -n "$QNX_HOST" ]; then
    echo "You need to specify QNX_HOST and QNX_TARGET"
    exit 1
  fi
fi

if [ -n "$FRIDA_ENV_NAME" ]; then
  frida_env_name_prefix=${FRIDA_ENV_NAME}-
else
  frida_env_name_prefix=
fi

pushd $releng_path/../ > /dev/null
FRIDA_ROOT=`pwd`
popd > /dev/null
FRIDA_BUILD="${FRIDA_BUILD:-$FRIDA_ROOT/build}"
FRIDA_RELENG="$FRIDA_ROOT/releng"
FRIDA_PREFIX="${FRIDA_PREFIX:-$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}}"
FRIDA_PREFIX_LIB="$FRIDA_PREFIX/lib"
FRIDA_TOOLROOT="$FRIDA_BUILD/${frida_env_name_prefix}toolchain-${build_os_arch}"
FRIDA_SDKROOT="$FRIDA_BUILD/${frida_env_name_prefix}sdk-${host_os_arch}"

if [ -n "$FRIDA_TOOLCHAIN_VERSION" ]; then
  toolchain_version=$FRIDA_TOOLCHAIN_VERSION
else
  toolchain_version=$(grep "frida_deps_version =" "$FRIDA_RELENG/deps.mk" | awk '{ print $NF }')
fi
if [ -n "$FRIDA_SDK_VERSION" ]; then
  sdk_version=$FRIDA_SDK_VERSION
else
  sdk_version=$(grep "frida_deps_version =" "$FRIDA_RELENG/deps.mk" | awk '{ print $NF }')
fi
if [ "$enable_asan" == "yes" ]; then
  sdk_version="$sdk_version-asan"
fi

detect_vala_api_version ()
{
  vala_api_version=$(ls -1 "$FRIDA_TOOLROOT/share" | grep "vala-" | cut -f2 -d"-")
}

if ! grep -Eq "^$toolchain_version\$" "$FRIDA_TOOLROOT/VERSION.txt" 2>/dev/null; then
  rm -rf "$FRIDA_TOOLROOT"
  mkdir -p "$FRIDA_TOOLROOT"

  filename=toolchain-$build_os-$build_arch.tar.bz2

  local_toolchain=$FRIDA_BUILD/_$filename
  if [ -f $local_toolchain ]; then
    echo -e "Deploying local toolchain \\033[1m$(basename $local_toolchain)\\033[0m..."
    tar -C "$FRIDA_TOOLROOT" -xjf $local_toolchain || exit 1
  else
    echo -e "Downloading and deploying toolchain for \\033[1m$build_os_arch\\033[0m..."
    $download_command "https://build.frida.re/deps/$toolchain_version/$filename" | tar -C "$FRIDA_TOOLROOT" -xjf -
    if [ $? -ne 0 ]; then
      echo ""
      echo "Bummer. It seems we don't have a prebuilt toolchain for your system."
      echo ""
      echo "Please go ahead and build it yourself:"
      echo "$ make -f Makefile.toolchain.mk"
      echo ""
      echo "Afterwards just retry and the toolchain will get picked up automatically."
      echo ""
      exit 2
    fi
  fi

  for template in $(find $FRIDA_TOOLROOT -name "*.frida.in"); do
    target=$(echo $template | sed 's,\.frida\.in$,,')
    cp -a "$template" "$target"
    sed \
      -e "s,@FRIDA_TOOLROOT@,$FRIDA_TOOLROOT,g" \
      -e "s,@FRIDA_RELENG@,$FRIDA_RELENG,g" \
      "$template" > "$target"
  done

  detect_vala_api_version

  vala_wrapper=$FRIDA_TOOLROOT/bin/valac-$vala_api_version
  vala_impl=$FRIDA_TOOLROOT/bin/valac-$vala_api_version-impl
  mv "$vala_wrapper" "$vala_impl"
  (
    echo "#!/bin/sh"
    echo "exec \"$vala_impl\" --target-glib=2.56 \"\$@\" --vapidir=\"$FRIDA_TOOLROOT/share/vala-$vala_api_version/vapi\""
  ) > "$vala_wrapper"
  chmod 755 "$vala_wrapper"
else
  detect_vala_api_version
fi

if [ "$FRIDA_ENV_SDK" != 'none' ] && ! grep -Eq "^$sdk_version\$" "$FRIDA_SDKROOT/VERSION.txt" 2>/dev/null; then
  rm -rf "$FRIDA_SDKROOT"
  mkdir -p "$FRIDA_SDKROOT"

  filename=sdk-$host_os-$host_arch.tar.bz2

  local_sdk=$FRIDA_BUILD/$filename
  if [ -f $local_sdk ]; then
    echo -e "Deploying local SDK \\033[1m$(basename $local_sdk)\\033[0m..."
    tar -C "$FRIDA_SDKROOT" -xjf $local_sdk || exit 1
  else
    echo -e "Downloading and deploying SDK for \\033[1m$host_os_arch\\033[0m..."
    $download_command "https://build.frida.re/deps/$sdk_version/$filename" | tar -C "$FRIDA_SDKROOT" -xjf - 2> /dev/null
    if [ $? -ne 0 ]; then
      echo ""
      echo "Bummer. It seems we don't have a prebuilt SDK for your system."
      echo ""
      echo "Please go ahead and build it yourself:"
      echo "$ make -f Makefile.sdk.mk"
      echo ""
      echo "Afterwards just retry and the SDK will get picked up automatically."
      echo ""
      exit 3
    fi
  fi

  for template in $(find $FRIDA_SDKROOT -name "*.frida.in"); do
    target=$(echo $template | sed 's,\.frida\.in$,,')
    cp -a "$template" "$target"
    sed \
      -e "s,@FRIDA_SDKROOT@,$FRIDA_SDKROOT,g" \
      -e "s,@FRIDA_RELENG@,$FRIDA_RELENG,g" \
      "$template" > "$target"
  done
fi

if [ -f "$FRIDA_SDKROOT/lib/c++/libc++.a" ]; then
  have_static_libcxx=yes
else
  have_static_libcxx=no
fi

selected_libtool=""
strip_flags=""

selected_otool=""

selected_cflags=""
selected_cxxflags=""
selected_cppflags=""
selected_ldflags=""

meson_common_flags="[]"
meson_objc=""
meson_objcpp=""
meson_linker_flavor=""

meson_platform_properties=()

flags_to_args ()
{
  if [ -n "$1" ]; then
    echo "'$(echo "$1" | sed "s/ /', '/g")'"
  else
    echo ""
  fi
}

mkdir -p "$FRIDA_BUILD"

if [ "$host_arch" == "arm64eoabi" ]; then
  export DEVELOPER_DIR="$XCODE11/Contents/Developer"
fi

xcrun="xcrun"
if [ "$build_os_arch" == "macos-arm64" ]; then
  if xcrun --show-sdk-path 2>&1 | grep -q "compatible arch"; then
    xcrun="arch -x86_64 xcrun"
  fi
fi

case $host_os in
  linux)
    host_arch_flags=""
    host_cflags=""
    case $host_arch in
      x86)
        host_arch_flags="-m32 -march=pentium4"
        host_cflags="-mfpmath=sse -mstackrealign"
        host_toolprefix="/usr/bin/"
        ;;
      x86_64)
        host_arch_flags="-m64"
        host_toolprefix="/usr/bin/"
        ;;
      arm)
        host_arch_flags="-march=armv5t"
        host_toolprefix="arm-linux-$frida_libc-"

        meson_host_cpu="armv5t"
        ;;
      armbe8)
        host_arch_flags="-march=armv6 -mbe8"
        host_toolprefix="armeb-linux-$frida_libc-"
        meson_host_cpu="armv6t"
        ;;
      armhf)
        host_arch_flags="-march=armv7-a+fp"
        host_toolprefix="arm-linux-$frida_libc-"

        meson_host_cpu="armv7a"
        ;;
      arm64)
        host_arch_flags="-march=armv8-a"
        host_toolprefix="aarch64-linux-$frida_libc-"
        ;;
      mips)
        host_arch_flags="-march=mips1 -mfp32"
        host_toolprefix="mips-linux-$frida_libc-"

        meson_host_cpu="mips1"
        ;;
      mipsel)
        host_arch_flags="-march=mips1 -mfp32"
        host_toolprefix="mipsel-linux-$frida_libc-"

        meson_host_cpu="mips1"
        ;;
      mips64)
        host_arch_flags="-march=mips64r2 -mabi=64"
        host_toolprefix="mips64-linux-$frida_libc-"

        meson_host_cpu="mips64r2"
        ;;
      mips64el)
        host_arch_flags="-march=mips64r2 -mabi=64"
        host_toolprefix="mips64el-linux-$frida_libc-"

        meson_host_cpu="mips64r2"
        ;;
      s390x)
        host_arch_flags="-march=z10 -m64"
        host_toolprefix="s390x-linux-$frida_libc-"
        ;;
    esac

    libgcc_flags="-static-libgcc"
    libstdcxx_flags="-static-libstdc++"
    base_compiler_flags="-ffunction-sections -fdata-sections"
    base_linker_flags="-Wl,--gc-sections -Wl,-z,noexecstack $libgcc_flags"

    if [ -n "$host_arch_flags" ]; then
      base_compiler_flags="$base_compiler_flags $host_arch_flags"
      base_linker_flags="$base_linker_flags $host_arch_flags"
    fi
    if [ -n "$host_cflags" ]; then
      base_compiler_flags="$base_compiler_flags $host_cflags"
    fi

    cc_config_flags="$libgcc_flags"
    cxx_config_flags="$libgcc_flags $libstdcxx_flags"

    selected_ld=$(read_toolchain_variable LD ${host_toolprefix}ld)
    if "$selected_ld" --version | grep -q "GNU gold"; then
      cc_config_flags="$cc_config_flags -fuse-ld=gold"
      cxx_config_flags="$cxx_config_flags -fuse-ld=gold"
      meson_linker_flavor=gold
      base_linker_flags="-Wl,--icf=all $base_linker_flags"
    fi

    selected_cc=$(read_toolchain_variable CC ${host_toolprefix}gcc)
    selected_cc="$selected_cc $cc_config_flags"
    eval tokens=($selected_cc)
    meson_c="${tokens[0]}"

    selected_cxx=$(read_toolchain_variable CXX ${host_toolprefix}g++)
    selected_cxx="$selected_cxx $cxx_config_flags"
    eval tokens=($selected_cxx)
    meson_cpp="${tokens[0]}"

    selected_ar=$(read_toolchain_variable AR ${host_toolprefix}ar)
    selected_nm=$(read_toolchain_variable NM ${host_toolprefix}nm)
    selected_ranlib=$(read_toolchain_variable RANLIB ${host_toolprefix}ranlib)
    selected_strip=$(read_toolchain_variable STRIP ${host_toolprefix}strip)
    strip_flags="--strip-all"
    selected_readelf=$(read_toolchain_variable READELF ${host_toolprefix}readelf)
    selected_objcopy=$(read_toolchain_variable OBJCOPY ${host_toolprefix}objcopy)
    selected_objdump=$(read_toolchain_variable OBJDUMP ${host_toolprefix}objdump)

    selected_cflags="$base_compiler_flags"
    selected_ldflags="$base_linker_flags"

    base_compiler_args=$(flags_to_args "$base_compiler_flags")
    base_linker_args=$(flags_to_args "$base_linker_flags")

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, $(flags_to_args "$libstdcxx_flags")"
    ;;
  macos)
    case $host_arch in
      arm64|arm64e)
        macos_minver="11.0"
        ;;
      *)
        macos_minver="10.9"
        ;;
    esac

    macos_sdk="macosx"
    if [ $host_arch = x86 ] && [ -n "$MACOS_X86_SDK_ROOT" ]; then
      macos_sdk_path="$MACOS_X86_SDK_ROOT"
    else
      macos_sdk_path="$($xcrun --sdk $macos_sdk --show-sdk-path)"
    fi

    clang_cc="$($xcrun --sdk $macos_sdk -f clang)"
    clang_cxx="$($xcrun --sdk $macos_sdk -f clang++)"

    cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang
    sed \
      -e "s,@driver@,$clang_cc,g" \
      -e "s,@sysroot@,$macos_sdk_path,g" \
      -e "s,@arch@,$host_clang_arch,g" \
      "$FRIDA_RELENG/driver-wrapper-xcode-default.sh.in" > "$cc_wrapper"
    chmod +x "$cc_wrapper"

    cxx_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang++
    if [ $have_static_libcxx = yes ] && [ $enable_asan = no ]; then
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$macos_sdk_path,g" \
        -e "s,@arch@,$host_clang_arch,g" \
        -e "s,@frida_sdkroot@,$FRIDA_SDKROOT,g" \
        "$FRIDA_RELENG/driver-wrapper-xcode-static-libc++.sh.in" > "$cxx_wrapper"
    else
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$macos_sdk_path,g" \
        -e "s,@arch@,$host_clang_arch,g" \
        "$FRIDA_RELENG/driver-wrapper-xcode-default.sh.in" > "$cxx_wrapper"
    fi
    chmod +x "$cxx_wrapper"

    ar_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-ar
    sed \
      -e "s,@ar@,$($xcrun --sdk $macos_sdk -f ar),g" \
      -e "s,@libtool@,$($xcrun --sdk $macos_sdk -f libtool),g" \
      "$FRIDA_RELENG/ar-wrapper-xcode.sh.in" > "$ar_wrapper"
    chmod +x "$ar_wrapper"

    selected_cc="$cc_wrapper"
    selected_cxx="$cxx_wrapper"
    selected_objc="$cc_wrapper"
    selected_objcxx="$cxx_wrapper"
    selected_ld="$($xcrun --sdk $macos_sdk -f ld)"

    selected_ar="$ar_wrapper"
    selected_nm="$($xcrun --sdk $macos_sdk -f llvm-nm)"
    selected_ranlib="$($xcrun --sdk $macos_sdk -f ranlib)"
    selected_libtool="$($xcrun --sdk $macos_sdk -f libtool)"
    selected_strip="$($xcrun --sdk $macos_sdk -f strip)"
    strip_flags="-Sx"

    install_name_tool="$($xcrun --sdk $macos_sdk -f install_name_tool)"
    selected_otool="$($xcrun --sdk $macos_sdk -f otool)"
    selected_codesign="$($xcrun --sdk $macos_sdk -f codesign)"
    selected_lipo="$($xcrun --sdk $macos_sdk -f lipo)"

    selected_cppflags="-mmacosx-version-min=$macos_minver"
    selected_cxxflags="-stdlib=libc++"
    selected_ldflags="-isysroot $macos_sdk_path -arch $host_clang_arch -Wl,-dead_strip"

    base_toolchain_args="'-mmacosx-version-min=$macos_minver'"
    base_compiler_args="$base_toolchain_args"
    base_linker_args="$base_toolchain_args, '-Wl,-dead_strip'"
    if [ $host_arch = x86 ]; then
      # Suppress linker warning about x86 being a deprecated architecture.
      base_linker_args="$base_linker_args, '-Wl,-w'"
    fi

    meson_c="$selected_cc"
    meson_cpp="$selected_cxx"
    meson_objc="$selected_cc"
    meson_objcpp="$selected_cxx"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args, '-stdlib=libc++'"
    meson_objc_args="$base_compiler_args"
    meson_objcpp_args="$base_compiler_args, '-stdlib=libc++'"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, '-stdlib=libc++'"
    meson_objc_link_args="$base_linker_args"
    meson_objcpp_link_args="$base_linker_args, '-stdlib=libc++'"
    ;;
  ios)
    ios_minver="8.0"

    case $host_arch in
      x86|x86_64)
        ios_sdk="iphonesimulator"
        ;;
      *)
        ios_sdk="iphoneos"
        ;;
    esac
    if [ -z "$IOS_SDK_ROOT" ]; then
      ios_sdk_path="$($xcrun --sdk $ios_sdk --show-sdk-path)"
    else
      ios_sdk_path="$IOS_SDK_ROOT"
    fi

    clang_cc="$($xcrun --sdk $ios_sdk -f clang)"
    clang_cxx="$($xcrun --sdk $ios_sdk -f clang++)"

    case $host_clang_arch in
      arm)
        ios_arch=armv7
        ;;
      *)
        ios_arch=$host_clang_arch
        ;;
    esac

    cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang
    sed \
      -e "s,@driver@,$clang_cc,g" \
      -e "s,@sysroot@,$ios_sdk_path,g" \
      -e "s,@arch@,$ios_arch,g" \
      "$FRIDA_RELENG/driver-wrapper-xcode-default.sh.in" > "$cc_wrapper"
    chmod +x "$cc_wrapper"

    cxx_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang++
    if [ $have_static_libcxx = yes ] && [ $enable_asan = no ]; then
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$ios_sdk_path,g" \
        -e "s,@arch@,$ios_arch,g" \
        -e "s,@frida_sdkroot@,$FRIDA_SDKROOT,g" \
        "$FRIDA_RELENG/driver-wrapper-xcode-static-libc++.sh.in" > "$cxx_wrapper"
    else
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$ios_sdk_path,g" \
        -e "s,@arch@,$ios_arch,g" \
        "$FRIDA_RELENG/driver-wrapper-xcode-default.sh.in" > "$cxx_wrapper"
    fi
    chmod +x "$cxx_wrapper"

    ar_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-ar
    sed \
      -e "s,@ar@,$($xcrun --sdk $ios_sdk -f ar),g" \
      -e "s,@libtool@,$($xcrun --sdk $ios_sdk -f libtool),g" \
      "$FRIDA_RELENG/ar-wrapper-xcode.sh.in" > "$ar_wrapper"
    chmod +x "$ar_wrapper"

    selected_cc="$cc_wrapper"
    selected_cxx="$cxx_wrapper"
    selected_objc="$cc_wrapper"
    selected_objcxx="$cxx_wrapper"
    selected_ld="$($xcrun --sdk $ios_sdk -f ld)"

    selected_ar="$ar_wrapper"
    selected_nm="$($xcrun --sdk $ios_sdk -f llvm-nm)"
    selected_ranlib="$($xcrun --sdk $ios_sdk -f ranlib)"
    selected_libtool="$($xcrun --sdk $ios_sdk -f libtool)"
    selected_strip="$($xcrun --sdk $ios_sdk -f strip)"
    strip_flags="-Sx"

    install_name_tool="$($xcrun --sdk $ios_sdk -f install_name_tool)"
    selected_otool="$($xcrun --sdk $ios_sdk -f otool)"
    selected_codesign="$($xcrun --sdk $ios_sdk -f codesign)"
    selected_lipo="$($xcrun --sdk $ios_sdk -f lipo)"

    selected_cppflags="-miphoneos-version-min=$ios_minver"
    selected_cxxflags="-stdlib=libc++"
    selected_ldflags="-isysroot $ios_sdk_path -arch $ios_arch -Wl,-dead_strip"

    base_toolchain_args="'-miphoneos-version-min=$ios_minver'"
    base_compiler_args="$base_toolchain_args"
    base_linker_args="$base_toolchain_args, '-Wl,-dead_strip'"

    meson_c="$selected_cc"
    meson_cpp="$selected_cxx"
    meson_objc="$selected_cc"
    meson_objcpp="$selected_cxx"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args, '-stdlib=libc++'"
    meson_objc_args="$base_compiler_args"
    meson_objcpp_args="$base_compiler_args, '-stdlib=libc++'"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, '-stdlib=libc++'"
    meson_objc_link_args="$base_linker_args"
    meson_objcpp_link_args="$base_linker_args, '-stdlib=libc++'"
    ;;
  android)
    android_build_os=$(echo ${build_os} | sed 's,^macos$,darwin,')
    case $build_os in
      macos)
        # NDK does not yet support Apple Silicon.
        android_build_arch=x86_64
        ;;
      linux)
        # Linux NDK only supports x86_64.
        android_build_arch=x86_64
        ;;
      *)
        android_build_arch=${build_arch}
        ;;
    esac
    android_toolroot="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/${android_build_os}-${android_build_arch}"
    android_clang_version=$(ls -1 "$android_toolroot/lib64/clang/" | grep -E "^[0-9]" | head -1)

    host_arch_flags=""
    host_cflags=""
    host_cxxlibs="c++_static c++abi"
    case $host_arch in
      x86)
        android_api=19
        android_abi="x86"
        android_target="i686-none-linux-android${android_api}"
        android_clang_arch="i386"
        host_compiler_triplet="i686-linux-android"
        host_arch_flags="-march=pentium4"
        host_cflags="-mfpmath=sse -mstackrealign"
        host_ldflags=""
        host_cxxlibs="$host_cxxlibs android_support"
        ;;
      x86_64)
        android_api=21
        android_abi="x86_64"
        android_target="x86_64-none-linux-android${android_api}"
        android_clang_arch="x86_64"
        host_compiler_triplet="x86_64-linux-android"
        host_ldflags=""
        ;;
      arm)
        android_api=19
        android_abi="armeabi-v7a"
        android_target="armv7-none-linux-androideabi${android_api}"
        android_clang_arch="arm"
        host_compiler_triplet="armv7a-linux-androideabi"
        host_tooltriplet="arm-linux-androideabi"
        host_arch_flags="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
        host_ldflags="-Wl,--fix-cortex-a8"
        host_cxxlibs="$host_cxxlibs android_support"
        ;;
      arm64)
        android_api=21
        android_abi="arm64-v8a"
        android_target="aarch64-none-linux-android${android_api}"
        android_clang_arch="aarch64"
        host_compiler_triplet="aarch64-linux-android"
        host_ldflags=""
        ;;
    esac

    base_compiler_flags="-DANDROID -fPIC -ffunction-sections -fdata-sections"
    base_linker_flags="-Wl,--gc-sections -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now"

    if [ -n "$host_arch_flags" ]; then
      base_compiler_flags="$base_compiler_flags $host_arch_flags"
      base_linker_flags="$base_linker_flags $host_arch_flags"
    fi
    if [ -n "$host_cflags" ]; then
      base_compiler_flags="$base_compiler_flags $host_cflags"
    fi
    if [ -n "$host_ldflags" ]; then
      base_linker_flags="$base_linker_flags $host_ldflags"
    fi

    host_compiler_prefix="${host_compiler_triplet}${android_api}-"

    if [ -z "$host_tooltriplet" ]; then
      host_tooltriplet="$host_compiler_triplet"
    fi
    host_toolprefix="$host_tooltriplet-"

    elf_cleaner=$FRIDA_TOOLROOT/bin/frida-elf-cleaner

    cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang
    sed \
      -e "s,@driver@,${android_toolroot}/bin/clang,g" \
      -e "s,@ndkroot@,$ANDROID_NDK_ROOT,g" \
      -e "s,@toolroot@,$android_toolroot,g" \
      -e "s,@target@,$android_target,g" \
      -e "s,@tooltriplet@,$host_tooltriplet,g" \
      -e "s,@api@,$android_api,g" \
      -e "s,@abi@,$android_abi,g" \
      -e "s,@clang_version@,$android_clang_version,g" \
      -e "s,@clang_arch@,$android_clang_arch,g" \
      -e "s,@cxxlibs@,$host_cxxlibs,g" \
      -e "s,@elf_cleaner@,$elf_cleaner,g" \
      "$FRIDA_RELENG/driver-wrapper-android.sh.in" > "$cc_wrapper"
    chmod +x "$cc_wrapper"

    cxx_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-clang++
    sed \
      -e "s,@driver@,${android_toolroot}/bin/clang++,g" \
      -e "s,@ndkroot@,$ANDROID_NDK_ROOT,g" \
      -e "s,@toolroot@,$android_toolroot,g" \
      -e "s,@target@,$android_target,g" \
      -e "s,@tooltriplet@,$host_tooltriplet,g" \
      -e "s,@api@,$android_api,g" \
      -e "s,@abi@,$android_abi,g" \
      -e "s,@clang_version@,$android_clang_version,g" \
      -e "s,@clang_arch@,$android_clang_arch,g" \
      -e "s,@cxxlibs@,$host_cxxlibs,g" \
      -e "s,@elf_cleaner@,$elf_cleaner,g" \
      "$FRIDA_RELENG/driver-wrapper-android.sh.in" > "$cxx_wrapper"
    chmod +x "$cxx_wrapper"

    selected_cc="$cc_wrapper"
    selected_cxx="$cxx_wrapper"
    selected_ld="${android_toolroot}/bin/ld"

    selected_ar="${android_toolroot}/bin/llvm-ar"
    selected_nm="${android_toolroot}/bin/llvm-nm"
    selected_ranlib="${android_toolroot}/bin/llvm-ranlib"
    selected_strip="${android_toolroot}/bin/llvm-strip"
    strip_flags="--strip-all"
    selected_readelf="${android_toolroot}/bin/llvm-readelf"
    selected_objcopy="${android_toolroot}/bin/llvm-objcopy"
    selected_objdump="${android_toolroot}/bin/llvm-objdump"

    selected_cflags="$base_compiler_flags"
    selected_ldflags="$base_linker_flags"

    base_compiler_args=$(flags_to_args "$base_compiler_flags")
    base_linker_args=$(flags_to_args "$base_linker_flags")

    meson_c="$selected_cc"
    meson_cpp="$selected_cxx"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args"
    if [ $android_api -lt 24 ]; then
      meson_cpp_args="$meson_cpp_args, '-D_LIBCPP_HAS_NO_OFF_T_FUNCTIONS'"
    fi

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args"
    ;;
  freebsd)
    host_toolprefix="/usr/bin/"

    selected_cc=$(read_toolchain_variable CC ${host_toolprefix}clang)
    selected_cxx=$(read_toolchain_variable CXX ${host_toolprefix}clang++)
    selected_ld=$(read_toolchain_variable LD ${host_toolprefix}ld)

    selected_ar=$(read_toolchain_variable AR ${host_toolprefix}ar)
    selected_nm=$(read_toolchain_variable NM ${host_toolprefix}nm)
    selected_ranlib=$(read_toolchain_variable RANLIB ${host_toolprefix}ranlib)
    selected_strip=$(read_toolchain_variable STRIP ${host_toolprefix}strip)
    strip_flags="--strip-all"
    selected_readelf=$(read_toolchain_variable READELF ${host_toolprefix}readelf)
    selected_objcopy=$(read_toolchain_variable OBJCOPY ${host_toolprefix}objcopy)

    base_compiler_flags="-ffunction-sections -fdata-sections"
    base_linker_flags="-Wl,--gc-sections"

    selected_cflags="$base_compiler_flags"
    selected_ldflags="$base_linker_flags"

    base_compiler_args=$(flags_to_args "$base_compiler_flags")
    base_linker_args=$(flags_to_args "$base_linker_flags")

    meson_c="$selected_cc"
    meson_cpp="$selected_cxx"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args"

    meson_b_lundef=false
    ;;
  qnx)
    case $host_arch in
      x86)
        qnx_host=i486-pc-nto-qnx6.6.0
        qnx_sysroot=$QNX_TARGET/x86

        host_arch_flags="-march=i686"
        ;;
      armeabi)
        qnx_host=arm-unknown-nto-qnx6.5.0eabi
        qnx_sysroot=$QNX_TARGET/armle-v7

        host_arch_flags="-march=armv7-a -mno-unaligned-access"

        meson_host_cpu="armv7"
        ;;
      arm)
        qnx_host=arm-unknown-nto-qnx6.5.0
        qnx_sysroot=$QNX_TARGET/armle

        host_arch_flags="-march=armv6 -mno-unaligned-access"

        meson_host_cpu="armv6"
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

    toolchain_flags="--sysroot=$qnx_sysroot $host_arch_flags $qnx_preprocessor_flags"
    selected_cc="$qnx_toolchain_prefix-gcc $toolchain_flags -static-libgcc"
    selected_cxx="$qnx_toolchain_prefix-g++ $toolchain_flags -static-libgcc -static-libstdc++"
    selected_ld="$qnx_toolchain_prefix-ld --sysroot=$qnx_sysroot"

    selected_ar="$qnx_toolchain_prefix-ar"
    selected_nm="$qnx_toolchain_prefix-nm"
    selected_ranlib="$qnx_toolchain_prefix-ranlib"
    selected_strip="$qnx_toolchain_prefix-strip"
    strip_flags="--strip-all"
    selected_readelf="$qnx_toolchain_prefix-readelf"
    selected_objcopy="$qnx_toolchain_prefix-objcopy"
    selected_objdump="$qnx_toolchain_prefix-objdump"

    selected_cflags="-ffunction-sections -fdata-sections"
    selected_ldflags="-Wl,--gc-sections -L$(dirname $qnx_sysroot/lib/gcc/4.8.3/libstdc++.a)"

    arch_args=$(flags_to_args "$host_arch_flags")

    base_toolchain_args="'--sysroot=$qnx_sysroot', $arch_args, '-static-libgcc'"
    base_compiler_args="$base_toolchain_args, '-ffunction-sections', '-fdata-sections'"
    base_linker_args="$base_toolchain_args, '-Wl,--gc-sections'"

    meson_c="$qnx_toolchain_prefix-gcc"
    meson_cpp="$qnx_toolchain_prefix-g++"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args, '-static-libstdc++'"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, '-static-libstdc++', '-L$(dirname $qnx_sysroot/lib/gcc/4.8.3/libstdc++.a)'"
    ;;
esac

case $host_os_arch in
  linux-armhf|ios-arm|android-arm)
    meson_c_args="$meson_c_args, '-mthumb'"
    meson_cpp_args="$meson_cpp_args, '-mthumb'"
    if [ -n "$meson_objc" ]; then
      meson_objc_args="$meson_objc_args, '-mthumb'"
    fi
    if [ -n "$meson_objcpp" ]; then
      meson_objcpp_args="$meson_objcpp_args, '-mthumb'"
    fi
    ;;
esac

if [ -n "$FRIDA_EXTRA_LDFLAGS" ]; then
  selected_ldflags="$selected_ldflags $FRIDA_EXTRA_LDFLAGS"
  extra_link_args=$(flags_to_args "$FRIDA_EXTRA_LDFLAGS")
  meson_c_link_args="$meson_c_link_args, $extra_link_args"
  meson_cpp_link_args="$meson_cpp_link_args, $extra_link_args"
fi

if [ $enable_asan = yes ]; then
  sanitizer_flag="-fsanitize=address"
  meson_sanitizer_arg=$(flags_to_args "$sanitizer_flag")

  selected_cc="$selected_cc $sanitizer_flag"
  selected_cxx="$selected_cxx $sanitizer_flag"
  if [ -n "$selected_objc" ]; then
    selected_objc="$selected_objc $sanitizer_flag"
  fi
  if [ -n "$selected_objcxx" ]; then
    selected_objcxx="$selected_objcxx $sanitizer_flag"
  fi
  selected_ld="$selected_ld $sanitizer_flag"
  meson_c_args="$meson_c_args, $meson_sanitizer_arg"
  meson_cpp_args="$meson_cpp_args, $meson_sanitizer_arg"
fi

selected_cflags="-fPIC $selected_cflags"
selected_cxxflags="$selected_cflags${selected_cxxflags:+ $selected_cxxflags}"

if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  version_include="-include $FRIDA_ROOT/build/frida-version.h"
  selected_cppflags="$version_include $selected_cppflags"

  meson_version_include=", '-include', '$FRIDA_ROOT/build/frida-version.h'"
else
  meson_version_include=""
fi

selected_valac="$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-valac"
vala_impl="$FRIDA_TOOLROOT/bin/valac-$vala_api_version"
vala_flags="--vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  vala_flags="$vala_flags --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\""
fi
(
  echo "#!/bin/sh"
  echo "exec \"$vala_impl\" $vala_flags \"\$@\""
) > "$selected_valac"
chmod 755 "$selected_valac"

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

strip_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-strip
(
  echo "#!/bin/sh"
  echo "for arg in \"\$@\"; do"
  echo "  if echo \"\$arg\" | grep -Eq '\\.a\$'; then"
  echo "    exit 0"
  echo "  fi"
  echo "done"
  echo "exec \"$selected_strip\" $strip_flags \"\$@\""
) > "$strip_wrapper"
chmod 755 "$strip_wrapper"

pkg_config_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_os_arch}-pkg-config

case $host_os in
  freebsd)
    libdatadir=libdata
    ;;
  *)
    libdatadir=lib
    ;;
esac

pkg_config="$FRIDA_TOOLROOT/bin/pkg-config"
pkg_config_flags="--static"
pkg_config_path="$FRIDA_PREFIX/$libdatadir/pkgconfig"
if [ "$FRIDA_ENV_NAME" == 'frida_gir' ]; then
  pkg_config_path="$(pkg-config --variable pc_path pkg-config):$pkg_config_path"
  pkg_config_flags=""
fi
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  pkg_config_flags=" $pkg_config_flags --define-variable=frida_sdk_prefix=$FRIDA_SDKROOT"
  pkg_config_path="$pkg_config_path:$FRIDA_SDKROOT/$libdatadir/pkgconfig"
fi
(
  echo "#!/bin/sh"
  echo "export PKG_CONFIG_PATH=\"$pkg_config_path\""
  echo "exec \"$pkg_config\" $pkg_config_flags \"\$@\""
) > "$pkg_config_wrapper"
chmod 755 "$pkg_config_wrapper"

env_rc=${FRIDA_BUILD}/${FRIDA_ENV_NAME:-frida}-env-${host_os_arch}.rc

if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  env_path_sdk="$FRIDA_SDKROOT/bin/${build_os_arch}"
  case ${build_os_arch} in
    linux-x86_64)
      env_path_sdk="$env_path_sdk:$FRIDA_SDKROOT/bin/linux-x86"
      ;;
    macos-arm64*)
      env_path_sdk="$env_path_sdk:$FRIDA_SDKROOT/bin/macos-x86_64"
      ;;
  esac
  env_path_sdk="$env_path_sdk:"
else
  env_path_sdk=""
fi

(
  echo "export PATH=\"${env_path_sdk}${FRIDA_TOOLROOT}/bin:\$PATH\""
  echo "export PKG_CONFIG=\"$pkg_config_wrapper\""
  echo "export PKG_CONFIG_PATH=\"$pkg_config_path\""
  echo "export VALAC=\"$selected_valac\""
  echo "export CPPFLAGS=\"$selected_cppflags\""
  echo "export CC=\"$selected_cc\""
  echo "export CFLAGS=\"$selected_cflags\""
  echo "export CXX=\"$selected_cxx\""
  echo "export CXXFLAGS=\"$selected_cxxflags\""
  echo "export LDFLAGS=\"$selected_ldflags\""
  echo "export AR=\"$selected_ar\""
  echo "export NM=\"$selected_nm\""
  echo "export STRIP=\"$strip_wrapper\""
) > $env_rc

case $host_os in
  macos|ios)
    (
      echo "export INSTALL_NAME_TOOL=\"$install_name_tool\""
      echo "export OTOOL=\"$selected_otool\""
      echo "export CODESIGN=\"$selected_codesign\""
      echo "export LIPO=\"$selected_lipo\""
      echo "export OBJC=\"$selected_objc\""
      echo "export OBJCXX=\"$selected_objcxx\""
      echo "export OBJCFLAGS=\"$selected_cflags\""
      echo "export OBJCXXFLAGS=\"$selected_cxxflags\""
    ) >> $env_rc
    ;;
esac

if [ -n "$meson_linker_flavor" ]; then
  (
    echo "export CC_LD=$meson_linker_flavor"
    echo "export CXX_LD=$meson_linker_flavor"
  ) >> $env_rc
  [ -n "$meson_objc" ] && echo "export OBJC_LD=$meson_linker_flavor" >> $env_rc
  [ -n "$meson_objcpp" ] && echo "export OBJCXX_LD=$meson_linker_flavor" >> $env_rc
fi

case $host_os in
  macos)
    (
      echo "export MACOSX_DEPLOYMENT_TARGET=$macos_minver"
    ) >> $env_rc
    ;;
esac

meson_machine_file=${FRIDA_BUILD}/${FRIDA_ENV_NAME:-frida}-${host_os_arch}.txt

(
  echo "[constants]"
  echo "common_flags = $meson_common_flags"
  echo ""
  echo "[binaries]"
  echo "c = ['$meson_c'${meson_version_include}]"
  echo "cpp = ['$meson_cpp'${meson_version_include}]"
  if [ -n "$meson_objc" ]; then
    echo "objc = ['$meson_objc'${meson_version_include}]"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp = ['$meson_objcpp'${meson_version_include}]"
  fi
  if [ -n "$meson_linker_flavor" ]; then
    echo "c_ld = '$meson_linker_flavor'"
    echo "cpp_ld = '$meson_linker_flavor'"
    [ -n "$meson_objc" ] && echo "objc_ld = '$meson_linker_flavor'"
    [ -n "$meson_objcpp" ] && echo "objcpp_ld = '$meson_linker_flavor'"
  fi
  echo "vala = '$selected_valac'"
  echo "ar = '$selected_ar'"
  echo "nm = '$selected_nm'"
  if [ -n "$selected_readelf" ]; then
    echo "readelf = '$selected_readelf'"
  fi
  if [ -n "$selected_objcopy" ]; then
    echo "objcopy = '$selected_objcopy'"
  fi
  if [ -n "$selected_objdump" ]; then
    echo "objdump = '$selected_objdump'"
  fi
  if [ -n "$install_name_tool" ]; then
    echo "install_name_tool = '$install_name_tool'"
  fi
  if [ -n "$selected_otool" ]; then
    echo "otool = '$selected_otool'"
  fi
  if [ -n "$selected_libtool" ]; then
    echo "libtool = '$selected_libtool'"
  fi
  echo "strip = '$strip_wrapper'"
  echo "pkgconfig = '$pkg_config_wrapper'"
  if [ $host_os_arch != $build_os_arch ] && [ -n "$FRIDA_QEMU_SYSROOT" ]; then
    case $host_arch in
      arm|armeabi|armhf)
        qemu=qemu-arm
        ;;
      armbe8)
        qemu=qemu-armeb
        ;;
      arm64)
        qemu=qemu-aarch64
        ;;
      *)
        qemu=qemu-$host_arch
        ;;
    esac
    echo "exe_wrapper = ['$qemu', '-L', '$FRIDA_QEMU_SYSROOT']"
  fi
  echo ""
  echo "[built-in options]"
  echo "c_args = common_flags + [${meson_c_args}]"
  echo "cpp_args = common_flags + [${meson_cpp_args}]"
  if [ -n "$meson_objc" ]; then
    echo "objc_args = common_flags + [${meson_objc_args}]"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp_args = common_flags + [${meson_objcpp_args}]"
  fi
  echo "c_link_args = common_flags + [$meson_c_link_args]"
  echo "cpp_link_args = common_flags + [$meson_cpp_link_args]"
  if [ -n "$meson_objc" ]; then
    echo "objc_link_args = common_flags + [$meson_objc_link_args]"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp_link_args = common_flags + [$meson_objcpp_link_args]"
  fi
  echo "b_lundef = $meson_b_lundef"
  echo ""
  echo "[properties]"
  if [ $host_os != $build_os ]; then
    echo "needs_exe_wrapper = true"
    echo ""
  fi
  if [ ${#meson_platform_properties[@]} -gt 0 ]; then
    echo ""
    for prop in "${meson_platform_properties[@]}"; do
      echo "$prop"
    done
  fi
  echo ""
  echo "[host_machine]"
  echo "system = '$meson_host_system'"
  echo "cpu_family = '$meson_host_cpu_family'"
  echo "cpu = '$meson_host_cpu'"
  echo "endian = '$meson_host_endian'"
) > $meson_machine_file
