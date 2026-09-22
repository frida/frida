---
title: Release 0.39
short-description: Release notes for 0.39
...

The 0.39.0 release turned out to consist almost entirely of bug fixes
and minor polishes.

# New features

## Extra arguments for tests

The Meson test executable allows specifying extra command line
arguments to pass to test executables.

    mesontest --test-args=--more-debug-info currenttest
