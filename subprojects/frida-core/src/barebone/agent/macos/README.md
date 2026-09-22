# The agent as a kext

The XNU flavour of the barebone agent is ordinarily a blob: a host with a debugger writes it into
a running kernel, tells it where that kernel keeps the things it calls, and starts it. The Linux
flavour can also be built as a module the kernel loads (`../linux`), and this is the same thing for
XNU -- a kext macOS loads.

What the two packagings share is everything the agent does. What differs is how it is put there,
how it is told about the kernel around it, and how the host reaches it.

## Building

Three things, in order. First an SDK for this kernel -- picolibc, GLib, QuickJS, capstone and the
rest, built for `macos-arm64-kernel`:

    cd frida-core
    python3 -m releng.deps build --host macos-arm64-kernel

That fetches the same sources every other flavour is built from, six of them pinned to a commit
that says what a Darwin kernel is not; see below. Then a GumJS devkit against it:

    cd frida-gum
    ./configure --host=macos-arm64-kernel --enable-gumjs --disable-v8 --with-devkits=gumjs
    make

Then the kext itself:

    make FRIDA_SDK=.../deps/sdk-macos-arm64-kernel \
         GUMJS_DEVKIT_DIR=.../frida-gum/build/bindings/gumjs/devkit

which builds the agent as a static library for the kernel's ABI, `frida-kext.c` against the Kernel
framework's headers, and links the two with the devkit into `frida-agent.kext`.

`make skeleton` builds the glue alone, which is enough to check the pipeline without the rest.

## How it is told about the kernel

A kext may call what the kernel exports to kexts, and the agent calls a great deal more than that:
`proc_find` and `copyin` are exported, `task_threads` and `mach_vm_allocate` are not. So the
addresses arrive the way they do for the blob -- worked out by whoever knows this kernel -- and are
written in through `/dev/frida` before the agent is started:

    ioctl(fd, FRIDA_IOC_SET_ADDR, &(FridaAddrRequest){ .name = "task_threads", .address = ... });
    ...
    ioctl(fd, FRIDA_IOC_START);

A name nothing writes stays zero, and the agent already answers for that: every one of them is an
`Option` on the Rust side, and what needs it says so rather than calling through a null.

Once started, the same device carries the frames: read what the agent says, write what the host
says, framed with a four-byte little-endian length exactly as on every other transport.

## What the sources needed

An SDK for a Darwin kernel is a target nothing here had been built for, and six of the sources say
so. Each is a commit of its own on the fork, a few lines apiece:

- **picolibc** -- Mach-O has no `__attribute__((alias))`, so the aliasing macros fall through to
  the assembler ones it already carries for Mach-O, and the names are spent before being spelled.
  Its aarch64 assembly is written for ELF and its C stubs answer for it instead. The interrupt
  vectors and the `exec` family are not built: one owns a machine this does not, the other crashes
  the compiler.
- **compiler-rt** -- `apple_versioning.c` is stubs for macOS releases long past, written against a
  header only userspace has.
- **libffi** -- the trampoline table is built out of what userspace has of Mach; whether that is
  there is now asked of the header rather than of the system's name.
- **QuickJS**, **tinycc** and **libiconv** -- all three ask what to do on Apple by asking whether
  they are on Apple. Where that means userspace -- a userspace malloc's header, a cache-control
  header, a locale one -- they now ask whether they are freestanding as well.

## Which architecture, and where pointers are signed

Apple silicon runs an arm64e kernel and loads nothing else, so that is what `ARCH` defaults to,
and the SDK and the devkit are built for `macos-arm64e-kernel` to match.

Pointer authentication is the thing to get right there, and what makes it awkward is that Rust's
`arm64e-apple-darwin` is a tag rather than an ABI: it neither signs a function pointer nor
authenticates one.

    _pick:  adrp x0, 0 ; add x0, x0, #0x0 ; ret        <- no signing
    _call:  mov x2, x0 ; mov x0, x1 ; br x2            <- no authenticating

So the line is drawn where the kernel is, and not inside:

- **Ourselves.** GLib, GumJS and the agent are built `-fno-ptrauth-calls`, so the C half calls a
  Rust callback the way Rust hands it over -- plainly. Nothing there signs, so nothing there is
  disappointed. `otool` says as much: not one `blraa` outside `__auth_stubs`.
- **The kernel calling us.** The glue keeps the kernel's own ABI, so the device table it registers
  is written with `AUTH` relocations and the loader signs it as the kernel expects.
- **Us calling the kernel.** Through `__auth_stubs`, which the loader fills the same way.
- **Us handing the kernel a function to call.** A thread's entry is the one that matters, and the
  agent signs it itself, as it already does for the kernel it is injected into. What it signs with
  is no longer written down twice: the glue derives the discriminator from the kernel's own
  headers with `ptrauth_type_discriminator`, and asserts that it is the number the blob flavour
  carries -- 0xd507 for a `thread_continue_t`, which is what it was.

What this does not buy is pointer authentication *within* the kext, which is protection given up
rather than correctness lost. Getting it back wants rustc to grow the ABI, or every callback
wrapped in a C thunk.

None of this touches x86_64, which has no pointer authentication at all: an Intel kext wants only
its own SDK built the same way.

## Floating point

Every other kernel flavour is built soft-float, since a kernel does not save the floating-point
registers across a trap. This one cannot be: Darwin has no soft-float C ABI on either
architecture. arm64 refuses the type outright without one -- `ABI 'darwinpcs' does not support
it` -- and `-mabi=aapcs-soft` alongside it crashes both Apple's clang and upstream LLVM on the
first varargs function taking a double; x86_64 wants SSE to return one. A script engine is nothing
but doubles, so the floating-point registers stay in play, and what that costs at run time is a
thing to answer on the machine.

## Putting copies into processes

Not there yet. The blob's linker script marks out its own writable half and what a copy of it has
to relocate; a kext is laid out by the kernel's loader instead, so the glue says there is nothing
to copy from. Everything the agent does in the kernel itself is unaffected.

Carrying the blob's ELF and placing it the way the host does is not a way round that, either: on a
machine with SPTM, memory a kext allocates cannot be made executable, while a kext's own text is
mapped executable by the kernel's loader because it was signed.
