# Linux kernel module

The same agent that the Barebone backend injects into XNU from the outside, but
loaded from the inside as an ordinary kernel module.

## How it differs from the XNU flavor

|                   | XNU                                              | Linux                                     |
|-------------------|--------------------------------------------------|-------------------------------------------|
| Delivery          | remote stub injects a freestanding ELF           | `insmod frida-agent.ko`                   |
| Kernel primitives | addresses patched into `.kernel_addrs` by the host | direct calls into `frida-kmod.c`        |
| Bootstrap config  | GVariant blob written into guest memory          | module parameters                         |
| Transport         | virtio hostlink or vsock                         | `/dev/frida` character device             |
| Page permissions  | host rewrites descriptors over the hostlink      | `set_memory_*()`                          |
| Writable alias    | shadow copy, committed via physical-memory bridge | `vmap()` of the same pages               |
| Symbols           | table supplied by the host                       | kallsyms                                  |

Because the Linux agent performs its own memory work, it never issues the
`RemapWritablePages`, `MemoryProtect` or `PatchCode` host RPCs. The wire protocol
is otherwise identical, so the host side sees the same `(yqv)` frames.

## Building

Four inputs: this repository, a GumJS devkit built for
`none-<arch>-softfloat_nopic`, the SDK it was built against, and the kernel the module is for. `Makefile`
covers x86_64 and arm64, and defaults to the architecture of the machine building.

Building for the running kernel is a good deal shorter than cross-building for a
device, so that walkthrough comes first; the arm64 one below it is the same steps
with the kernel obtained from elsewhere.

### Toolchain

    rustup target add x86_64-unknown-none    # or aarch64-unknown-none
    rustup component add rust-src
    sudo dnf install clang binutils kernel-devel-$(uname -r)

clang must be 19 or newer, since that is where `-mabi=aapcs-soft` landed; GCC does
not implement it at any version, nor does it reach x86_64's soft-float ABI. The
binutils are for the prelink, which reads the archive map that only GNU `nm` prints —
the LLVM one does not; cross-building wants the target's, natively the host's own
will do. The host compiler is for Cargo's own build scripts, which run on the machine
doing the building.

The commands below are run from the top of this repository, with `releng` checked
out — `git submodule update --init releng` if the clone did not recurse.

### For the running x86_64 kernel

No prebuilt bundles are published for `none-x86_64-softfloat_nopic`, so both inputs are
built locally, against the same SDK:

    releng/deps.py build --bundle=sdk --host=none-x86_64-softfloat_nopic
    mkdir -p ~/sdk-none-x86_64-softfloat_nopic
    tar -C ~/sdk-none-x86_64-softfloat_nopic -xf deps/sdk-none-x86_64-softfloat_nopic.tar.xz

    gum=subprojects/frida-gum
    mkdir -p $gum/build/none-x86_64-softfloat_nopic
    (cd $gum/build/none-x86_64-softfloat_nopic \
        && FRIDA_DEPS=$PWD/../../../../deps ../../configure \
            --host=none-x86_64-softfloat_nopic \
            --enable-gumjs \
            --with-devkits=gumjs \
            --with-devkit-symbol-scope=original \
        && make)

    make -C src/barebone/agent/linux \
        FRIDA_SDK=$HOME/sdk-none-x86_64-softfloat_nopic \
        GUMJS_DEVKIT_DIR=$PWD/$gum/build/none-x86_64-softfloat_nopic/bindings/gumjs/devkit

Everything else — `KDIR`, the Rust target, the binutils, the kernel's own name for the
architecture — follows from the running kernel.

### For an arm64 device

Here there are published bundles to start from, and the kernel has to be assembled.

#### The devkit and the SDK

    version=17.17.0
    base=https://github.com/frida/frida/releases/download/$version
    curl -LO $base/frida-gumjs-devkit-$version-none-arm64-softfloat_nopic.tar.xz
    mkdir -p ~/gumjs-devkit
    tar -C ~/gumjs-devkit -xf frida-gumjs-devkit-$version-none-arm64-softfloat_nopic.tar.xz

    releng/deps.py sync sdk none-arm64-softfloat_nopic ~/sdk-none-arm64-softfloat_nopic

The SDK carries picolibc and the compiler-rt builtins the devkit expects to be linked
against, and `releng/deps.toml` pins which one. Mixing a devkit with an SDK built for
a different libc leaves undefined symbols at prelink time.

#### The kernel

Nothing here needs the device's own kernel to be built, but the config alone is not
enough either. `uname -r` ends in the GKI commit and the build number:

    adb shell uname -r
    6.1.145-android14-11-gc1de4747ac59-ab14219743

which is commit `c1de4747ac59` and build `14219743`. Three pieces are keyed to them:

    commit=c1de4747ac59
    build=14219743
    ci=https://ci.android.com/builds/submitted/$build/kernel_aarch64/latest/raw

    mkdir -p ~/kernel-prepared ~/kernel-source
    curl -sSL $ci/modules_prepare_outdir.tar.gz | tar -xz -C ~/kernel-prepared
    curl -sSL $ci/kernel_aarch64_Module.symvers -o ~/kernel-prepared/Module.symvers
    curl -sSL https://android.googlesource.com/kernel/common/+archive/$commit.tar.gz \
        | tar -xz -C ~/kernel-source

`modules_prepare_outdir.tar.gz` is the configured build tree with `scripts/` already
compiled as x86-64 binaries, so the kbuild half has to run on an x86-64 Linux host —
an arm64 one gets as far as `CC [M]` and then fails to run `fixdep`. `Module.symvers`
carries the CRCs `CONFIG_MODVERSIONS` checks. The top-level `Makefile` in that prepared tree is a stub
that includes the source's, hence both `KDIR` and `KOUT` below.

A different GKI build of the same branch does not substitute for these: vermagic is
compared verbatim, and it names the exact build.

#### Build

    make -C src/barebone/agent/linux \
        FRIDA_SDK=$HOME/sdk-none-arm64-softfloat_nopic \
        GUMJS_DEVKIT_DIR=$HOME/gumjs-devkit \
        AGENT_LD=aarch64-linux-gnu-ld \
        AGENT_AR=aarch64-linux-gnu-ar \
        AGENT_NM=aarch64-linux-gnu-nm \
        AGENT_OBJCOPY=aarch64-linux-gnu-objcopy \
        KDIR=$HOME/kernel-source \
        KOUT=$HOME/kernel-prepared \
        LLVM=1

`make` builds the Rust staticlib itself, prelinks it against the devkit and the SDK,
then hands the result to kbuild. The `AGENT_*` overrides are only needed because the
defaults name an `aarch64-none-elf-` GNU toolchain; drop them if that is what is
installed.

The prelink half runs anywhere; the kbuild half needs the x86-64 Linux host above.
Cross-building from macOS means running that step in a container holding the kernel
tree.

    adb push src/barebone/agent/linux/frida-agent.ko /data/local/tmp/

## Loading

    insmod frida-agent.ko

The module's init path returns as soon as the worker thread is running — watch
`dmesg` for `frida: listening on /dev/frida`. `rmmod frida-agent` unwinds in the
reverse order and does not return until the worker is off the module's text.

## Talking to it

The agent exposes `/dev/frida` and waits: one client at a time, `read`/`write` of the
same length-prefixed frames the other transports carry, with `poll` for readiness.
It never dials out, so there is no network namespace to get right and no ordering
requirement between loading the module and attaching a client.

On Android the node is created by devtmpfs without a policy-specific label, so an
SELinux-enforcing system needs the client's domain granted access to it before it can
open the device. A socket-based transport was the other option considered and is a
worse fit: `AF_VSOCK` only reaches a peer when a hypervisor is on the other side, and
having the kernel connect out to loopback needs something already listening before
the module loads.

### Reaching it from a host

Serve the Barebone device from a frida-server running on the target, so the `open()`
happens next to the device node and the host talks to an ordinary frida-server:

    FRIDA_BAREBONE_CONFIG=/data/local/tmp/linux-kmod.json \
        frida-server --device barebone -l 127.0.0.1:27042

`etc/linux-kmod.json` is that config. From the host, with `adb forward tcp:27042
tcp:27042` in place:

    frida -H 127.0.0.1:27042 -p 0

## Injecting into userspace

Loaded on a live system the module doubles as a ptrace-free injector for ordinary
processes — how the Linux local backend reaches its targets when the module is
present, falling back to `ptrace` when it is not. There is no second device node;
the channel is `prctl()` with a magic option and a token in the fifth argument,

    prctl(0x46524944, op, arg1, arg2, 0x1d5f9e6b2c7a4038)

and any call whose option or token does not match falls through to the real
prctl, so probing for the module looks like a stock kernel. `op` 32 pings and
returns the magic.

Allocate, free, read, write and spawn — the primitives the injector drives
against a target pid — require `CAP_SYS_ADMIN`. A spawned thread is a real
`CLONE_THREAD` sibling that adopts the target's mm, files, credentials,
namespaces, fs and SysV semaphore undo list, so it is a native member of the
group rather than a kernel thread wearing its address space.

Cloaking is Gum's own registry mirrored into the kernel so it holds against
`/proc` too: the module intercepts the `maps`, `smaps`, task and status readers
and drops cloaked threads, ranges and fds while keeping the counts consistent. A
process cloaks its own resources with the token alone — no capability — so an
injected agent self-cloaks by forwarding Gum's updates over the same channel,
and the loader adds the footprint the agent itself never sees. A cloaked thread
still sees through the cloak when it reads `/proc`, so the agent is not blind to
its own process.

## What the target kernel has to provide

- **x86_64 or arm64.** The register-level pieces — the page size, cache maintenance,
  where the syscall ABI leaves prctl()'s arguments, and how a spawned thread's
  registers are set up for its return to userspace — have a half for each. The XNU
  flavor remains arm64-only.
- **`CONFIG_KPROBES` and `CONFIG_KALLSYMS_ALL`.** A good deal of what the shim
  needs is compiled in but not exported — `kallsyms_lookup_name`,
  `kallsyms_on_each_symbol` and `set_memory_*` all are, and GKI trims its export
  table to the KMI symbol list on top of that. The shim resolves them by name
  through a kprobe, which is the standard way back in. Without kallsyms the module
  still loads but cannot look up symbols or change page permissions.
- **Unsigned modules.** `CONFIG_MODULE_SIG_FORCE` must be off. Android's
  `CONFIG_MODULE_SIG_PROTECT` is fine: it only demands signatures for modules on
  the GKI list.

## Kernel ABI constraints

A hardened kernel imposes an ABI on anything linked into it, and the FP constraint
below is why this flavor is built against a soft-float SDK
(`none-arm64-softfloat_nopic`, `none-x86_64-softfloat_nopic`) rather than an ordinary
bare-metal one. The remaining constraints are per-architecture.

- **FP/SIMD.** JavaScript numbers are doubles, so a hardfloat build would use FP
  for as long as the runtime lives, and neither architecture lets a kernel thread
  hold it for free — x86 wants every stretch bracketed by `kernel_fpu_begin()`, and
  on arm64 kernels before 6.9 there is no correct way to hold it at all:
  `kernel_neon_begin()` runs with preemption disabled so we cannot sleep under it,
  and the scheduler does not save a kernel thread's FP registers — live values are
  lost on preemption and the FP state of whichever user task was interrupted is
  corrupted. Building everything soft-float removes the question rather than
  answering it: `-mabi=aapcs-soft -mgeneral-regs-only` on arm64, and on x86_64
  LLVM's `+soft-float`, which is also what Rust's own `x86_64-unknown-none` turns
  on, so the two halves agree on where a double travels. `-mno-sse` alone does not
  reach the ABI; and `long double` has to be narrowed to 64 bits with
  `-mlong-double-64`, since soft-float has no lowering for x87's 80-bit format and
  LLVM crashes rather than calling out to a builtin.
- **Indirect branch tracking (x86_64).** With `CONFIG_X86_KERNEL_IBT` an indirect
  call landing on anything but an `endbr64` faults, so everything linked in is built
  `-fcf-protection=branch` and the Rust half `-Zcf-protection=branch`.
- **Kernel code model and red zone (x86_64).** Modules are loaded into the top 2GB
  of the address space and their relocations are resolved as if by `-mcmodel=kernel`,
  and there is no red zone below the stack pointer for an interrupt to step on. Both
  come with the Rust target; the C half is told explicitly.
- **objtool (x86_64).** It validates every object entering a module and builds its
  ORC unwind tables, and was not written to read a JavaScript runtime linked against
  a freestanding libc. `Kbuild` marks the prelinked blob `OBJECT_FILES_NON_STANDARD`.
- **Shadow call stack (arm64).** With `CONFIG_SHADOW_CALL_STACK` the kernel reserves x18, so
  the SDK and Gum are built `-ffixed-x18` and the Rust half with `-Zfixed-x18`.
  Anything linked in that uses x18 as a scratch register overwrites the kernel's SCS
  pointer, and the thread then returns through whatever that address happens to hold.

  The Rust flag only reaches crates Cargo compiles, so `core`, `alloc` and
  `compiler_builtins` — which the toolchain ships prebuilt, and which between them
  account for every x18 write that used to reach the module — are rebuilt from source
  via `-Z build-std`. That needs `rustup component add rust-src`.

## The soft-float libc

The libc is picolibc and the compiler runtime is compiler-rt, both from the
soft-float SDK that `FRIDA_SDK` points at. `Makefile` trims a copy of its
`libc.a` before the prelink: the agent implements `malloc`, the C library's locks and
its process stubs over the kernel's own facilities, so picolibc's are duplicate
definitions rather than fallbacks. The set is derived from what the agent defines
rather than listed, since picolibc moves things between releases.

On arm64 those compiler-rt builtins are load-bearing in a way that is easy to miss.
Rust's toolchain carries its own `compiler_builtins`, compiled for the ordinary AAPCS
where a double travels in `d0`, and the prelink whole-archives the Rust staticlib — so
without the filtering step in `Makefile` those definitions answer the C library's calls
to `__muldf3` and friends, which pass the same double in `x0`. Nothing crashes. The
arithmetic simply returns garbage, and the first thing to notice is GLib deciding a
hash table need not grow. x86_64 needs none of that filtering: soft-float is part of
the Rust target there, so the copy `-Z build-std` compiles already agrees with the C
half.

## What a script may look like

The runtime compiles scripts on a kernel thread's stack, and QuickJS parses
expressions by recursion, so the ceiling is nesting rather than length: roughly
fourteen levels of parentheses or fifteen of object literals on arm64 Linux, whose
kernel stack is 16 KiB — as is x86_64's. A script that exceeds it fails to compile rather than taking
anything down, though the message is not always `stack overflow` — an over-deep
`if`/`else` chain reports `expecting ';'`. Long scripts are fine; deep ones are not.
