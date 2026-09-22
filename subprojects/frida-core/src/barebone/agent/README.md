## Getting the SDK and the devkit

The agent is built against the bare soft-float SDK, which carries picolibc and the
compiler runtime, and against a GumJS devkit built for the same flavor. Which flavor
is the target's, listed under Targets below; this walks through the XNU one. No
prebuilt bundles are published for these, so build both:

    machine=none-arm64-softfloat_nopic

    releng/deps.py build --bundle=sdk --host=$machine
    mkdir -p ~/sdk-$machine
    tar -C ~/sdk-$machine -xf deps/sdk-$machine.tar.xz

    gum=~/src/frida-gum
    mkdir -p $gum/build/$machine
    (cd $gum/build/$machine \
        && FRIDA_DEPS=$HOME/src/frida-core/deps CC=clang CXX=clang++ AR=llvm-ar \
            RANLIB=llvm-ranlib NM=llvm-nm STRIP=llvm-strip \
            ../../configure --host=$machine --enable-gumjs \
                --with-devkits=gumjs \
        && python3 ../../releng/meson/meson.py configure -Ddevkit_symbol_scope=original \
        && ninja)

## Building

    sdk=~/sdk-$machine
    export GUMJS_DEVKIT_DIR=$gum/build/$machine/bindings/gumjs/devkit
    export CC="clang -I$sdk/include -I$sdk/include/glib-2.0 -I$sdk/lib/glib-2.0/include \
        -I$sdk/include/capstone -I$sdk/include/json-glib-1.0"
    export RUSTFLAGS="-L native=$sdk/lib"

    RUSTC_BOOTSTRAP=1 cargo build --release --features xnu \
        --target aarch64-unknown-none-softfloat --target-dir target-xnu \
        -Z build-std=core,alloc,compiler_builtins

Rust names no bare-metal target for the Windows guests, so the agent spells them
out and they are asked for by path:

    RUSTC_BOOTSTRAP=1 cargo build --release --features win9x \
        --target ./i686-unknown-none-pic.json -Z json-target-spec \
        --target-dir target-win9x -Z build-std=core,alloc,compiler_builtins

## Targets

The agent supports four kernels. One of them has to be picked; none is a default,
since none follows from the host you build on:

- `xnu` — a freestanding ELF executable that the Barebone backend
  injects into a running kernel from the outside, over a remote stub.
- `win9x` — the same, for a 32-bit Windows 9x guest.
- `winnt` — the same, for a Windows NT guest, either word size, on x86 or arm64.
- `linux` — a static library that becomes a kernel module, loaded from the
  inside. See `linux/README.md`.

Each flavor has an SDK and a devkit of its own, since the addressing differs: an
image the host places goes in position-independent, a Linux module is absolute,
and NT's 64-bit half is called in the Microsoft ABI.

| Flavor  | Machine                           | Rust target                      |
|---------|-----------------------------------|----------------------------------|
| `xnu`   | `none-arm64-softfloat_nopic`      | `aarch64-unknown-none-softfloat` |
| `win9x` | `none-x86-softfloat_pic`          | `i686-unknown-none-pic.json`     |
| `winnt` | `none-x86-softfloat_pic`          | `i686-unknown-none-pic.json`     |
| `winnt` | `none-x86_64-softfloat_pic_msabi` | `x86_64-unknown-none-pic.json`   |
| `winnt` | `none-arm64-softfloat_pic`        | `aarch64-unknown-none-pic.json`  |
| `linux` | `none-<arch>-softfloat_nopic`     | see `linux/README.md`            |

Everything above the `kernel` module is shared; each backend supplies the same
set of primitives (logging, allocation, threads, waiting, time, symbols) plus its
half of Gum's platform backend (`gum_xnu.rs` / `gum_windows.rs` / `gum_linux.rs`).

## Development loop

    export FRIDA_BAREBONE_CONFIG=$PWD/etc/xnu.json
    cargo build --release --features xnu --target aarch64-unknown-none-softfloat \
        && make -C ~/src/frida-python \
        && killall -9 qemu-system-aarch64 && sleep 2 \
        && frida -D barebone -p 0

## Speeding up loop

    ./configure \
        -- \
        -Dfrida-core:compat=disabled \
        -Dfrida-core:local_backend=disabled \
        -Dfrida-core:simmy_backend=disabled \
        -Dfrida-core:fruity_backend=disabled \
        -Dfrida-core:droidy_backend=disabled \
        -Dfrida-core:socket_backend=disabled \
        -Dfrida-core:compiler_backend=disabled \
        -Dfrida-core:gadget=disabled \
        -Dfrida-core:server=disabled \
        -Dfrida-core:portal=disabled \
        -Dfrida-core:inject=disabled \
        -Dfrida-core:tests=enabled
