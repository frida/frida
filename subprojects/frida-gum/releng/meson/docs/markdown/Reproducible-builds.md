# Reproducible builds

A reproducible build means the following (as quoted from [the
reproducible builds project site](https://reproducible-builds.org/)):

> Reproducible builds are a set of software development practices that
  create a verifiable path from human readable source code to the
  binary code used by computers.

Roughly what this means is that if two different people compile the
project from source, their outputs are bitwise identical to each
other. This allows people to verify that binaries downloadable from
the net actually come from the corresponding sources and have not, for
example, had malware added to them.

Meson aims to support reproducible builds out of the box with zero
additional work (assuming the rest of the build environment is set up
for reproducibility). If you ever find a case where this is not
happening, it is a bug. Please file an issue with as much information
as possible and we'll get it fixed.
