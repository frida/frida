// The XNU flavour ships as a freestanding ELF executable: the host injects the
// image and calls its `_start`. All of the agent lives in the library crate;
// this target exists only to produce that link. build.rs passes `--undefined
// _start` so the entrypoint survives --gc-sections.
//
// The Linux flavour is built as a static library instead and linked into a
// kernel module by kbuild — see linux/Kbuild.

#![no_std]
#![no_main]

use frida_barebone_agent as _;
