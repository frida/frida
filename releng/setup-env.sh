#!/bin/bash

releng_path=`dirname $0`

build_platform=$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,macos,')
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
if [ -n "$FRIDA_LIBC" ]; then
  libc=$FRIDA_LIBC
else
  libc=glibc
fi
host_clang_arch=$(echo -n $host_arch | sed 's,^x86$,i386,')
host_platform_arch=${host_platform}-${host_arch}

meson_host_system=$(echo $host_platform | sed 's,^macos$,darwin,' | sed 's,^ios$,darwin,' | sed 's,^android$,linux,')
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
  arm64)
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
    meson_host_cpu_family=mips
    meson_host_cpu=mips
    meson_host_endian=big
    ;;
  *)
    meson_host_cpu_family=$host_arch
    meson_host_cpu=$host_arch
    meson_host_endian=little
    ;;
esac

case $FRIDA_ASAN in
  yes|no)
    enable_asan=$FRIDA_ASAN
    ;;
  *)
    enable_asan=no
    ;;
esac

if which curl &>/dev/null; then
  download_command="curl -sS"
elif which wget &>/dev/null; then
  download_command="wget -O - -q"
else
  echo "Please install curl or wget: required for downloading SDK and toolchain." > /dev/stderr
  exit 1
fi
case $build_platform in
  linux)
    tar_stdin=""
    ;;
  macos)
    tar_stdin="-"
    ;;
  *)
    echo "Could not determine build platform" > /dev/stderr
    exit 1
esac

if [ -z "$FRIDA_HOST" ]; then
  echo "Assuming host is $host_platform_arch Set FRIDA_HOST to override."
fi

if [ $host_platform = android ]; then
  ndk_required_name="r20"
  if [ -n "$ANDROID_NDK_ROOT" ]; then
    if [ -f "$ANDROID_NDK_ROOT/source.properties" ]; then
      ndk_installed_version=$(grep Pkg.Revision "$ANDROID_NDK_ROOT/source.properties" | awk '{ split($NF, v, "."); print v[1]; }')
    else
      ndk_installed_version=$(cut -f1 -d" " "$ANDROID_NDK_ROOT/RELEASE.TXT")
    fi
    if [ $ndk_installed_version -ne 20 ]; then
      (
        echo ""
        echo "Unsupported NDK version $ndk_installed_version. Please install NDK $ndk_required_name."
        echo ""
        echo "Frida's SDK - the prebuilt dependencies snapshot - was compiled against $ndk_required_name,"
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
    echo "ANDROID_NDK_ROOT must be set to the location of your $ndk_required_name NDK." > /dev/stderr
    exit 1
  fi
fi

if [ $host_platform = qnx ]; then
  if [ ! -n "$QNX_HOST" ]; then
    echo "You need to specify QNX_HOST and QNX_TARGET"
    exit 1
  fi
fi

prompt_color=33

toolchain_version=20190428
sdk_version=20190731
if [ $enable_asan = yes ]; then
  sdk_version="$sdk_version-asan"
fi

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

OBJCOPY=""
OBJDUMP=""
LIBTOOL=""
STRIP_FLAGS=""

OTOOL=""

CFLAGS=""
CXXFLAGS=""
CPPFLAGS=""
LDFLAGS=""

meson_root=""

meson_objc=""
meson_objcpp=""

meson_platform_properties=()

flags_to_args () {
  if [ -n "$1" ]; then
    echo "'$(echo "$1" | sed "s/ /', '/g")'"
  else
    echo ""
  fi
}

mkdir -p "$FRIDA_BUILD"

case $host_platform in
  linux)
    case $host_arch in
      x86)
        host_arch_flags="-m32"
        host_toolprefix="/usr/bin/"
        ;;
      x86_64)
        host_arch_flags="-m64"
        host_toolprefix="/usr/bin/"
        ;;
      arm)
        host_arch_flags="-march=armv5t"
        host_toolprefix="arm-linux-gnueabi-"

        meson_host_cpu="armv5t"
        ;;
      armhf)
        host_arch_flags="-march=armv6"
        host_toolprefix="arm-linux-gnueabihf-"

        meson_host_cpu="armv6hf"
        ;;
      arm64)
        host_arch_flags="-march=armv8-a"
        host_toolprefix="aarch64-linux-gnu-"
        ;;
      mips)
        host_arch_flags="-march=mips1"
        host_toolprefix="mips-unknown-linux-$libc-"

        meson_host_cpu="mips1"
        ;;
      mipsel)
        host_arch_flags="-march=mips1"
        host_toolprefix="mipsel-unknown-linux-$libc-"

        meson_host_cpu="mips1"
        ;;
      mips64)
        host_arch_flags="-march=mips64r2 -mabi=64"
        host_toolprefix="mips64-linux-gnuabi64-"

        meson_host_cpu="mips64r2"
        ;;
      mips64el)
        host_arch_flags="-march=mips64r2 -mabi=64"
        host_toolprefix="mips64el-linux-gnuabi64-"

        meson_host_cpu="mips64r2"
        ;;
    esac
    CPP="${host_toolprefix}cpp"
    CC="${host_toolprefix}gcc -static-libgcc"
    CXX="${host_toolprefix}g++ -static-libgcc -static-libstdc++"
    LD="${host_toolprefix}ld"

    AR="${host_toolprefix}ar"
    NM="${host_toolprefix}nm"
    RANLIB="${host_toolprefix}ranlib"
    STRIP="${host_toolprefix}strip"
    STRIP_FLAGS="--strip-all"
    OBJCOPY="${host_toolprefix}objcopy"
    OBJDUMP="${host_toolprefix}objdump"

    CFLAGS="$host_arch_flags -ffunction-sections -fdata-sections"
    LDFLAGS="$host_arch_flags -Wl,--gc-sections -Wl,-z,noexecstack"

    arch_args=$(flags_to_args "$host_arch_flags")

    base_toolchain_args="$arch_args, '-static-libgcc'"
    base_compiler_args="$base_toolchain_args, '-ffunction-sections', '-fdata-sections'"
    base_linker_args="$base_toolchain_args, '-Wl,--gc-sections', '-Wl,-z,noexecstack'"

    meson_c="${host_toolprefix}gcc"
    meson_cpp="${host_toolprefix}g++"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args, '-static-libstdc++'"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, '-static-libstdc++'"
    ;;
  macos)
    macos_minver="10.9"

    macos_sdk="macosx"
    macos_sdk_path="$(xcrun --sdk $macos_sdk --show-sdk-path)"

    clang_cc="$(xcrun --sdk $macos_sdk -f clang)"
    clang_cxx="$(xcrun --sdk $macos_sdk -f clang++)"

    cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang
    sed \
      -e "s,@driver@,$clang_cc,g" \
      -e "s,@sysroot@,$macos_sdk_path,g" \
      -e "s,@arch@,$host_clang_arch,g" \
      "$releng_path/driver-wrapper-xcode-default.sh.in" > "$cc_wrapper"
    chmod +x "$cc_wrapper"

    cxx_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang++
    if [ "$FRIDA_ENV_SDK" != 'none' ] && [ $enable_asan = no ]; then
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$macos_sdk_path,g" \
        -e "s,@arch@,$host_clang_arch,g" \
        -e "s,@frida_sdkroot@,$FRIDA_SDKROOT,g" \
        "$releng_path/driver-wrapper-xcode-static-libc++.sh.in" > "$cxx_wrapper"
    else
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$macos_sdk_path,g" \
        -e "s,@arch@,$host_clang_arch,g" \
        "$releng_path/driver-wrapper-xcode-default.sh.in" > "$cxx_wrapper"
    fi
    chmod +x "$cxx_wrapper"

    ar_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-ar
    sed \
      -e "s,@ar@,$(xcrun --sdk $macos_sdk -f ar),g" \
      -e "s,@libtool@,$(xcrun --sdk $macos_sdk -f libtool),g" \
      "$releng_path/ar-wrapper-xcode.sh.in" > "$ar_wrapper"
    chmod +x "$ar_wrapper"

    CPP="$cc_wrapper -E"
    CC="$cc_wrapper"
    CXX="$cxx_wrapper"
    OBJC="$cc_wrapper"
    OBJCXX="$cxx_wrapper"
    LD="$(xcrun --sdk $macos_sdk -f ld)"

    AR="$ar_wrapper"
    NM="$FRIDA_ROOT/releng/llvm-nm-macos-x86_64"
    RANLIB="$(xcrun --sdk $macos_sdk -f ranlib)"
    LIBTOOL="$(xcrun --sdk $macos_sdk -f libtool)"
    STRIP="$(xcrun --sdk $macos_sdk -f strip)"
    STRIP_FLAGS="-Sx"

    INSTALL_NAME_TOOL="$(xcrun --sdk $macos_sdk -f install_name_tool)"
    OTOOL="$(xcrun --sdk $macos_sdk -f otool)"
    CODESIGN="$(xcrun --sdk $macos_sdk -f codesign)"
    LIPO="$(xcrun --sdk $macos_sdk -f lipo)"

    CPPFLAGS="-mmacosx-version-min=$macos_minver"
    CXXFLAGS="-stdlib=libc++"
    LDFLAGS="-isysroot $macos_sdk_path -Wl,-macosx_version_min,$macos_minver -arch $host_clang_arch -Wl,-dead_strip"

    meson_root="$macos_sdk_path"

    base_toolchain_args="'-mmacosx-version-min=$macos_minver'"
    base_compiler_args="$base_toolchain_args"
    base_linker_args="$base_toolchain_args, '-Wl,-dead_strip'"

    meson_c="$CC"
    meson_cpp="$CXX"
    meson_objc="$CC"
    meson_objcpp="$CXX"

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
    ios_minver="7.0"

    case $host_arch in
      x86|x86_64)
        ios_sdk="iphonesimulator"
        ;;
      *)
        ios_sdk="iphoneos"
        ;;
    esac
    ios_sdk_path="$(xcrun --sdk $ios_sdk --show-sdk-path)"

    clang_cc="$(xcrun --sdk $ios_sdk -f clang)"
    clang_cxx="$(xcrun --sdk $ios_sdk -f clang++)"

    case $host_clang_arch in
      arm)
        ios_arch=armv7
        ;;
      *)
        ios_arch=$host_clang_arch
        ;;
    esac

    cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang
    sed \
      -e "s,@driver@,$clang_cc,g" \
      -e "s,@sysroot@,$ios_sdk_path,g" \
      -e "s,@arch@,$ios_arch,g" \
      "$releng_path/driver-wrapper-xcode-default.sh.in" > "$cc_wrapper"
    chmod +x "$cc_wrapper"

    cxx_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang++
    if [ "$FRIDA_ENV_SDK" != 'none' ] && [ $enable_asan = no ]; then
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$ios_sdk_path,g" \
        -e "s,@arch@,$ios_arch,g" \
        -e "s,@frida_sdkroot@,$FRIDA_SDKROOT,g" \
        "$releng_path/driver-wrapper-xcode-static-libc++.sh.in" > "$cxx_wrapper"
    else
      sed \
        -e "s,@driver@,$clang_cxx,g" \
        -e "s,@sysroot@,$ios_sdk_path,g" \
        -e "s,@arch@,$ios_arch,g" \
        "$releng_path/driver-wrapper-xcode-default.sh.in" > "$cxx_wrapper"
    fi
    chmod +x "$cxx_wrapper"

    ar_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-ar
    sed \
      -e "s,@ar@,$(xcrun --sdk $ios_sdk -f ar),g" \
      -e "s,@libtool@,$(xcrun --sdk $ios_sdk -f libtool),g" \
      "$releng_path/ar-wrapper-xcode.sh.in" > "$ar_wrapper"
    chmod +x "$ar_wrapper"

    CPP="$cc_wrapper -E"
    CC="$cc_wrapper"
    CXX="$cxx_wrapper"
    OBJC="$cc_wrapper"
    OBJCXX="$cxx_wrapper"
    LD="$(xcrun --sdk $ios_sdk -f ld)"

    AR="$ar_wrapper"
    NM="$FRIDA_ROOT/releng/llvm-nm-macos-x86_64"
    RANLIB="$(xcrun --sdk $ios_sdk -f ranlib)"
    LIBTOOL="$(xcrun --sdk $ios_sdk -f libtool)"
    STRIP="$(xcrun --sdk $ios_sdk -f strip)"
    STRIP_FLAGS="-Sx"

    INSTALL_NAME_TOOL="$(xcrun --sdk $ios_sdk -f install_name_tool)"
    OTOOL="$(xcrun --sdk $ios_sdk -f otool)"
    CODESIGN="$(xcrun --sdk $ios_sdk -f codesign)"
    LIPO="$(xcrun --sdk $ios_sdk -f lipo)"

    CPPFLAGS="-miphoneos-version-min=$ios_minver"
    CXXFLAGS="-stdlib=libc++"
    LDFLAGS="-isysroot $ios_sdk_path -Wl,-iphoneos_version_min,$ios_minver -arch $ios_arch -Wl,-dead_strip"

    meson_root="$ios_sdk_path"

    base_toolchain_args="'-miphoneos-version-min=$ios_minver'"
    base_compiler_args="$base_toolchain_args"
    base_linker_args="$base_toolchain_args, '-Wl,-dead_strip'"

    meson_c="$CC"
    meson_cpp="$CXX"
    meson_objc="$CC"
    meson_objcpp="$CXX"

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
    android_build_platform=$(echo ${build_platform} | sed 's,^macos$,darwin,')
    android_toolroot="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/${android_build_platform}-${build_arch}"

    case $host_arch in
      x86)
        android_api=18
        host_compiler_triplet="i686-linux-android"
        host_arch_flags="-march=i686"
        host_ldflags="-fuse-ld=gold"
        ;;
      x86_64)
        android_api=21
        host_compiler_triplet="x86_64-linux-android"
        host_arch_flags=""
        host_ldflags="-fuse-ld=gold -Wl,--icf=all"
        ;;
      arm)
        android_api=18
        host_compiler_triplet="armv7a-linux-androideabi"
        host_tooltriplet="arm-linux-androideabi"
        host_arch_flags="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
        host_ldflags="-fuse-ld=gold -Wl,--icf=all -Wl,--fix-cortex-a8"
        ;;
      arm64)
        android_api=21
        host_compiler_triplet="aarch64-linux-android"
        host_arch_flags=""
        host_ldflags="-fuse-ld=gold -Wl,--icf=all"
        ;;
    esac
    host_compiler_prefix="${host_compiler_triplet}${android_api}-"

    if [ -z "$host_tooltriplet" ]; then
      host_tooltriplet="$host_compiler_triplet"
    fi
    host_toolprefix="$host_tooltriplet-"

    if [ $android_api -lt 21 ]; then
      # XXX: Meson's auto-detection fails as this is a Clang built-in.
      meson_platform_properties+=("has_function_stpcpy = false")
    fi

    CPP="${android_toolroot}/bin/${host_compiler_prefix}clang -E"
    CC="${android_toolroot}/bin/${host_compiler_prefix}clang"
    CXX="${android_toolroot}/bin/${host_compiler_prefix}clang++"
    LD="${android_toolroot}/bin/${host_toolprefix}ld"

    AR="${android_toolroot}/bin/${host_toolprefix}ar"
    NM="${android_toolroot}/bin/${host_toolprefix}nm"
    RANLIB="${android_toolroot}/bin/${host_toolprefix}ranlib"
    STRIP="${android_toolroot}/bin/${host_toolprefix}strip"
    STRIP_FLAGS="--strip-all"
    OBJCOPY="${android_toolroot}/bin/${host_toolprefix}objcopy"
    OBJDUMP="${android_toolroot}/bin/${host_toolprefix}objdump"

    CFLAGS="$host_arch_flags -DANDROID -ffunction-sections -fdata-sections"
    LDFLAGS="$host_arch_flags $host_ldflags -Wl,--gc-sections -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now"

    elf_cleaner=${FRIDA_ROOT}/releng/frida-elf-cleaner-${build_platform_arch}

    meson_cc_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang
    sed \
      -e "s,@driver@,${android_toolroot}/bin/${host_compiler_prefix}clang,g" \
      -e "s,@elf_cleaner@,$elf_cleaner,g" \
      "$releng_path/driver-wrapper-android.sh.in" > "$meson_cc_wrapper"
    chmod +x "$meson_cc_wrapper"

    meson_cpp_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-clang++
    sed \
      -e "s,@driver@,${android_toolroot}/bin/${host_compiler_prefix}clang++,g" \
      -e "s,@elf_cleaner@,$elf_cleaner,g" \
      "$releng_path/driver-wrapper-android.sh.in" > "$meson_cpp_wrapper"
    chmod +x "$meson_cpp_wrapper"

    base_toolchain_args=$(flags_to_args "$host_arch_flags")
    if [ -n "$base_toolchain_args" ]; then
      base_toolchain_args="$base_toolchain_args, "
    fi
    base_compiler_args="$base_toolchain_args'-DANDROID', '-fPIC', '-ffunction-sections', '-fdata-sections'"
    base_linker_args="$base_toolchain_args$(flags_to_args "$host_ldflags"), '-Wl,--gc-sections', '-Wl,-z,noexecstack', '-Wl,-z,relro', '-Wl,-z,now'"

    meson_c="$meson_cc_wrapper"
    meson_cpp="$meson_cpp_wrapper"

    meson_c_args="$base_compiler_args"
    meson_cpp_args="$base_compiler_args"

    meson_c_link_args="$base_linker_args"
    meson_cpp_link_args="$base_linker_args, '-static-libstdc++'"
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
    CPP="$qnx_toolchain_prefix-cpp $toolchain_flags"
    CC="$qnx_toolchain_prefix-gcc $toolchain_flags -static-libgcc"
    CXX="$qnx_toolchain_prefix-g++ $toolchain_flags -static-libgcc -static-libstdc++"
    LD="$qnx_toolchain_prefix-ld --sysroot=$qnx_sysroot"

    AR="$qnx_toolchain_prefix-ar"
    NM="$qnx_toolchain_prefix-nm"
    RANLIB="$qnx_toolchain_prefix-ranlib"
    STRIP="$qnx_toolchain_prefix-strip"
    STRIP_FLAGS="--strip-all"
    OBJCOPY="$qnx_toolchain_prefix-objcopy"
    OBJDUMP="$qnx_toolchain_prefix-objdump"

    CFLAGS="-ffunction-sections -fdata-sections"
    LDFLAGS="-Wl,--gc-sections -L$(dirname $qnx_sysroot/lib/gcc/4.8.3/libstdc++.a)"

    meson_root="$qnx_sysroot"

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

case $host_platform_arch in
  android-arm|ios-arm)
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
  LDFLAGS="$LDFLAGS $FRIDA_EXTRA_LDFLAGS"
  extra_link_args=$(flags_to_args "$FRIDA_EXTRA_LDFLAGS")
  meson_c_link_args="$meson_c_link_args, $extra_link_args"
  meson_cpp_link_args="$meson_cpp_link_args, $extra_link_args"
fi

# We need these legacy paths for dependencies that don't use pkg-config
legacy_includes="-I$FRIDA_PREFIX/include"
legacy_libpaths="-L$FRIDA_PREFIX/lib"
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  legacy_includes="$legacy_includes -I$FRIDA_SDKROOT/include"
  legacy_libpaths="$legacy_libpaths -L$FRIDA_SDKROOT/lib"
fi
CPPFLAGS="$CPPFLAGS $legacy_includes"
LDFLAGS="$LDFLAGS $legacy_libpaths"

meson_legacy_includes=$(flags_to_args "$legacy_includes")
meson_legacy_libpaths=$(flags_to_args "$legacy_libpaths")
meson_c_args="$meson_c_args, $meson_legacy_includes"
meson_cpp_args="$meson_cpp_args, $meson_legacy_includes"
meson_c_link_args="$meson_c_link_args, $meson_legacy_libpaths"
meson_cpp_link_args="$meson_cpp_link_args, $meson_legacy_libpaths"

if [ $enable_asan = yes ]; then
  sanitizer_flag="-fsanitize=address"

  CC="$CC $sanitizer_flag"
  CXX="$CXX $sanitizer_flag"
  if [ -n "$OBJC" ]; then
    OBJC="$OBJC $sanitizer_flag"
  fi
  if [ -n "$OBJCXX" ]; then
    OBJCXX="$OBJCXX $sanitizer_flag"
  fi
  LD="$LD $sanitizer_flag"
fi

CFLAGS="-fPIC $CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS"

if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  version_include="-include $FRIDA_BUILD/frida-version.h"
  CPPFLAGS="$version_include $CPPFLAGS"

  meson_version_include=", '-include', '$FRIDA_BUILD/frida-version.h'"
else
  meson_version_include=""
fi

ACLOCAL_FLAGS="-I $FRIDA_PREFIX/share/aclocal"
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $FRIDA_SDKROOT/share/aclocal"
fi
ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $FRIDA_TOOLROOT/share/aclocal"
ACLOCAL="aclocal $ACLOCAL_FLAGS"
CONFIG_SITE="$FRIDA_BUILD/${frida_env_name_prefix}config-${host_platform_arch}.site"

VALAC="$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-valac"
vala_impl="$FRIDA_TOOLROOT/bin/valac-0.46"
vala_flags="--vapidir=\"$FRIDA_PREFIX/share/vala/vapi\""
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  vala_flags="$vala_flags --vapidir=\"$FRIDA_SDKROOT/share/vala/vapi\""
fi
(
  echo "#!/bin/sh"
  echo "exec \"$vala_impl\" $vala_flags \"\$@\""
) > "$VALAC"
chmod 755 "$VALAC"

[ ! -d "$FRIDA_PREFIX/share/aclocal}" ] && mkdir -p "$FRIDA_PREFIX/share/aclocal"
[ ! -d "$FRIDA_PREFIX/lib}" ] && mkdir -p "$FRIDA_PREFIX/lib"

if ! grep -Eq "^$toolchain_version\$" "$FRIDA_TOOLROOT/.version" 2>/dev/null; then
  rm -rf "$FRIDA_TOOLROOT"
  mkdir -p "$FRIDA_TOOLROOT"

  local_toolchain=$FRIDA_BUILD/toolchain.tar.bz2
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

  vala_wrapper=$FRIDA_TOOLROOT/bin/valac-0.46
  vala_impl=$FRIDA_TOOLROOT/bin/valac-0.46-impl
  mv "$vala_wrapper" "$vala_impl"
  (
    echo "#!/bin/sh"
    echo "exec \"$vala_impl\" --target-glib=2.62 \"\$@\" --vapidir=\"$FRIDA_TOOLROOT/share/vala-0.46/vapi\""
  ) > "$vala_wrapper"
  chmod 755 "$vala_wrapper"

  ln -s "${FRIDA_ROOT}/releng/ninja-${build_platform_arch}" "$FRIDA_TOOLROOT/bin/ninja"
  ln -s "${FRIDA_ROOT}/releng/frida-resource-compiler-${build_platform_arch}" "$FRIDA_TOOLROOT/bin/frida-resource-compiler"

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

  echo $sdk_version > "$FRIDA_SDKROOT/.version"
fi

strip_wrapper=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-strip
(
  echo "#!/bin/sh"
  echo "for arg in \"\$@\"; do"
  echo "  if echo \"\$arg\" | egrep -q '.a\$'; then"
  echo "    exit 0"
  echo "  fi"
  echo "done"
  echo "exec \"$STRIP\" $STRIP_FLAGS \"\$@\""
) > "$strip_wrapper"
chmod 755 "$strip_wrapper"

PKG_CONFIG=$FRIDA_BUILD/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}-pkg-config

pkg_config="$FRIDA_TOOLROOT/bin/pkg-config"
pkg_config_flags=""
pkg_config_path="$FRIDA_PREFIX_LIB/pkgconfig"
if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  pkg_config_flags=" --define-variable=frida_sdk_prefix=$FRIDA_SDKROOT"
  pkg_config_path="$pkg_config_path:$FRIDA_SDKROOT/lib/pkgconfig"
fi
(
  echo "#!/bin/sh"
  echo "export PKG_CONFIG_PATH=\"$pkg_config_path\""
  echo "exec \"$pkg_config\"$pkg_config_flags --static \"\$@\""
) > "$PKG_CONFIG"
chmod 755 "$PKG_CONFIG"

env_rc=build/${FRIDA_ENV_NAME:-frida}-env-${host_platform_arch}.rc
meson_env_rc=build/${FRIDA_ENV_NAME:-frida}-meson-env-${host_platform_arch}.rc

if [ "$FRIDA_ENV_SDK" != 'none' ]; then
  env_path_sdk="$FRIDA_SDKROOT/bin:"
else
  env_path_sdk=""
fi

(
  echo "export PATH=\"${env_path_sdk}${FRIDA_TOOLROOT}/bin:\$PATH\""
  echo "export PS1=\"\e[0;${prompt_color}m[\u@\h \w \e[m\e[1;${prompt_color}mfrida-${host_platform_arch}\e[m\e[0;${prompt_color}m]\e[m\n\$ \""
  echo "export PKG_CONFIG=\"$PKG_CONFIG\""
  echo "export PKG_CONFIG_PATH=\"\""
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
  echo "export STRIP=\"$strip_wrapper\""
  echo "export ACLOCAL_FLAGS=\"$ACLOCAL_FLAGS\""
  echo "export ACLOCAL=\"$ACLOCAL\""
  echo "export CONFIG_SITE=\"$CONFIG_SITE\""
) > $env_rc

if [ -n "$OBJCOPY" ]; then
  echo "export OBJCOPY=\"$OBJCOPY\"" >> $env_rc
fi

if [ -n "$OBJDUMP" ]; then
  echo "export OBJDUMP=\"$OBJDUMP\"" >> $env_rc
fi

case $host_platform in
  macos|ios)
    (
      echo "export INSTALL_NAME_TOOL=\"$INSTALL_NAME_TOOL\""
      echo "export OTOOL=\"$OTOOL\""
      echo "export CODESIGN=\"$CODESIGN\""
      echo "export LIPO=\"$LIPO\""
      echo "export OBJC=\"$OBJC\""
      echo "export OBJCXX=\"$OBJCXX\""
      echo "export OBJCFLAGS=\"$CFLAGS\""
      echo "export OBJCXXFLAGS=\"$CXXFLAGS\""
    ) >> $env_rc
    ;;
esac

case $host_platform in
  macos)
    (
      echo "export MACOSX_DEPLOYMENT_TARGET=10.9"
    ) >> $env_rc
    ;;
esac

egrep -v "^export LD=" "$env_rc" > "$meson_env_rc"

sed \
  -e "s,@frida_host_platform@,$host_platform,g" \
  -e "s,@frida_host_arch@,$host_arch,g" \
  -e "s,@frida_host_platform_arch@,$host_platform_arch,g" \
  -e "s,@frida_prefix@,$FRIDA_PREFIX,g" \
  -e "s,@frida_optimization_flags@,$FRIDA_OPTIMIZATION_FLAGS,g" \
  -e "s,@frida_debug_flags@,$FRIDA_DEBUG_FLAGS,g" \
  -e "s,@frida_libc@,$libc,g" \
  $releng_path/config.site.in > "$CONFIG_SITE"

meson_cross_file=build/${FRIDA_ENV_NAME:-frida}-${host_platform_arch}.txt

(
  echo "[binaries]"
  echo "c = '$meson_c'"
  echo "cpp = '$meson_cpp'"
  if [ -n "$meson_objc" ]; then
    echo "objc = '$meson_objc'"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp = '$meson_objcpp'"
  fi
  echo "vala = '$VALAC'"
  echo "ar = '$AR'"
  echo "nm = '$NM'"
  if [ -n "$OBJDUMP" ]; then
    echo "objdump = '$OBJDUMP'"
  fi
  if [ -n "$INSTALL_NAME_TOOL" ]; then
    echo "install_name_tool = '$INSTALL_NAME_TOOL'"
  fi
  if [ -n "$OTOOL" ]; then
    echo "otool = '$OTOOL'"
  fi
  if [ -n "$LIBTOOL" ]; then
    echo "libtool = '$LIBTOOL'"
  fi
  echo "strip = '$strip_wrapper'"
  echo "pkgconfig = '$PKG_CONFIG'"
  echo ""
  echo "[properties]"
  if [ $host_platform != $build_platform ]; then
    echo "needs_exe_wrapper = true"
    echo ""
  fi
  if [ -n "$meson_root" ]; then
    echo "root = '$meson_root'"
    echo ""
  fi
  echo "c_args = [${meson_c_args}${meson_version_include}]"
  echo "cpp_args = [${meson_cpp_args}${meson_version_include}]"
  if [ -n "$meson_objc" ]; then
    echo "objc_args = [${meson_objc_args}${meson_version_include}]"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp_args = [${meson_objcpp_args}${meson_version_include}]"
  fi
  echo "c_link_args = [$meson_c_link_args]"
  echo "cpp_link_args = [$meson_cpp_link_args]"
  if [ -n "$meson_objc" ]; then
    echo "objc_link_args = [$meson_objc_link_args]"
  fi
  if [ -n "$meson_objcpp" ]; then
    echo "objcpp_link_args = [$meson_objcpp_link_args]"
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
) > $meson_cross_file

echo "Environment created. To enter:"
echo "# source ${FRIDA_ROOT}/$env_rc"
