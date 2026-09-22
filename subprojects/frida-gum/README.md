# Gum

Cross-platform instrumentation and introspection library written in C.

This library is consumed by [frida-core][] through its JavaScript bindings,
[GumJS][].

Provides:

- Instrumentation core
  - Inline hooking: [Interceptor][]
  - Stealthy code tracing: [Stalker][]
  - Memory monitoring: [MemoryAccessMonitor][]

- Cross-platform introspection
  - Running threads and other [process][] state
  - Loaded modules, including their:
    - Imports
    - Exports
    - Symbols
  - [Memory][] scanning
  - [DebugSymbol][] lookups
  - [Backtracer][] implementations
  - [Kernel][] state (iOS only for now)

- Out-of-process dynamic linker for i/macOS: [Gum.Darwin.Mapper][]

- Code generation:
  - [X86Writer][]
  - [ArmWriter][]
  - [ThumbWriter][]
  - [Arm64Writer][]
  - [MipsWriter][]

- Code relocation:
  - [X86Relocator][]
  - [ArmRelocator][]
  - [ThumbRelocator][]
  - [Arm64Relocator][]
  - [MipsRelocator][]

- Helper libraries for developers needing highly granular:

  - [Heap][] allocation tracking and leak checking.
  - [Profiling][] with [worst-case inspector][] callback.

## Build Instructions

Builds can be configured with the `configure` script. `configure.bat` is
available for Windows users.

```bash
./configure --enable-gumpp --enable-gumjs --enable-tests
make
```

Run configure with `--help` to see all options.

## Testing

Tests are built with `make test` and run with `./build/tests/gum-tests`.
You can specify a test path with `-p` to run a specific test or group of
tests.

Test paths follow the pattern `/<Area>/<Module>/<test_name>` where the
area/module come from TESTENTRY macros in the fixture files (e.g.
`TESTENTRY_WITH_FIXTURE("Core/Interceptor", ...)`).

```bash
./build/tests/gum-tests
./build/tests/gum-tests -p /Core/Interceptor/attach_one
```

## Binaries

Download a devkit for statically linking into your own projects from the
Frida [releases][] page.


[frida-core]: https://github.com/frida/frida-core
[GumJS]: https://github.com/frida/frida-gum/tree/master/bindings/gumjs
[Interceptor]: https://github.com/frida/frida-gum/blob/master/gum/guminterceptor.h
[Stalker]: https://github.com/frida/frida-gum/blob/master/gum/gumstalker.h
[MemoryAccessMonitor]: https://github.com/frida/frida-gum/blob/master/gum/gummemoryaccessmonitor.h
[process]: https://github.com/frida/frida-gum/blob/master/gum/gumprocess.h
[Memory]: https://github.com/frida/frida-gum/blob/master/gum/gummemory.h
[DebugSymbol]: https://github.com/frida/frida-gum/blob/master/gum/gumsymbolutil.h
[Backtracer]: https://github.com/frida/frida-gum/blob/master/gum/gumbacktracer.h
[Kernel]: https://github.com/frida/frida-gum/blob/master/gum/gumkernel.h
[Gum.Darwin.Mapper]: https://github.com/frida/frida-gum/blob/master/gum/backend-darwin/include/gum/gumdarwinmapper.h
[X86Writer]: https://github.com/frida/frida-gum/blob/master/gum/arch-x86/gumx86writer.h
[ArmWriter]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm/gumarmwriter.h
[ThumbWriter]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm/gumthumbwriter.h
[Arm64Writer]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm64/gumarm64writer.h
[MipsWriter]: https://github.com/frida/frida-gum/blob/master/gum/arch-mips/gummipswriter.h
[X86Relocator]: https://github.com/frida/frida-gum/blob/master/gum/arch-x86/gumx86relocator.h
[ArmRelocator]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm/gumarmrelocator.h
[ThumbRelocator]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm/gumthumbrelocator.h
[Arm64Relocator]: https://github.com/frida/frida-gum/blob/master/gum/arch-arm64/gumarm64relocator.h
[MipsRelocator]: https://github.com/frida/frida-gum/blob/master/gum/arch-mips/gummipsrelocator.h
[Heap]: https://github.com/frida/frida-gum/tree/master/libs/gum/heap
[Profiling]: https://github.com/frida/frida-gum/tree/master/libs/gum/prof
[worst-case inspector]: https://github.com/frida/frida-gum/blob/7e4c5b547b035ae05d2f9e160652101bf741e6c3/libs/gum/prof/gumprofiler.h#L40-L42
[releases]: https://github.com/frida/frida/releases
