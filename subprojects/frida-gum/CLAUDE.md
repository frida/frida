# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when
working with code in this repository.

## What is frida-gum

Cross-platform instrumentation library. Core of
[Frida](https://frida.re). Provides inline hooking (Interceptor),
code tracing (Stalker), memory scanning, process introspection, and
code generation/relocation for x86, ARM, ARM64, and MIPS. Written
in C using GLib/GObject. Has C++ bindings (Gumpp) and JavaScript
bindings (GumJS, with QuickJS and V8 runtimes).

## Build commands

```bash
./configure --enable-gumpp --enable-gumjs --enable-tests
make
make test
./build/tests/gum-tests
./build/tests/gum-tests /Core/Interceptor/attach_one
./build/tests/gum-tests -p /Core/Interceptor
```

Test paths follow the pattern `/<Area>/<Module>/<test_name>` where
the area/module come from TESTENTRY macros in the fixture files
(e.g. `TESTENTRY_WITH_FIXTURE("Core/Interceptor", ...)`).

## Code style rules

Enforced by `./tests/stylecheck.py` in CI on changed lines:
- **2-space indentation**, no tabs
- **80-column line limit**
- Space before parentheses in function calls: `func (arg)` not
  `func(arg)`
- Space after casts: `(GumAddress) ptr` not `(GumAddress)ptr`
- Space around pointer declarations: `Type * name` not
  `Type *name` or `Type* name`
- Opening brace on its **own line**, not same line as
  `if`/`for`/function signature
- No blank lines after `{` or before `}`
- No consecutive blank lines
- Multi-parameter function definitions: parameters aligned to
  opening `(`

## Commit message style

- Subject line: max 50 characters
- Body lines: wrap at 72 characters (use the full width, or
  slightly less if it avoids making the next line awkward)

## Architecture

**Core library** (`gum/`): Pure C, platform-agnostic API with
backend dispatch.
- `gum/arch-{x86,arm,arm64,mips}/` — Code writers and relocators
  for each ISA
- `gum/backend-{windows,darwin,linux,freebsd,qnx}/` — OS-specific
  implementations
- `gum/backend-{elf,libunwind,libdwarf,posix}/` — Format and
  library backends

**Bindings** (`bindings/`):
- `gumpp/` — C++ wrappers (C++14, RTTI disabled)
- `gumjs/` — JavaScript API for QuickJS and V8. Generated via
  `generate-bindings.py` and `generate-runtime.py`

**Tests** (`tests/`): GLib test framework. Each test module has a
`-fixture.c` file (included by the test `.c` file, not compiled
separately) that defines `TESTCASE`, `TESTENTRY`, fixtures, and
setup/teardown. Tests registered in `gumtest.c`.

## Key patterns

- **GObject type system**: `G_DECLARE_FINAL_TYPE`, `GUM_TYPE_*`
  macros, ref-counting with `g_object_ref/unref`
- **Naming**: types `GumInterceptor`, functions
  `gum_interceptor_attach`, files `guminterceptor.{h,c}`, public
  API marked `GUM_API`
- **Platform conditionals**: `HAVE_WINDOWS`, `HAVE_DARWIN`,
  `HAVE_LINUX`, `HAVE_ANDROID`, `HAVE_IOS`, `HAVE_QNX`,
  `HAVE_I386`, `HAVE_ARM`, `HAVE_ARM64`, `HAVE_MIPS`
- **Licence**: wxWindows Library Licence, Version 3.1
