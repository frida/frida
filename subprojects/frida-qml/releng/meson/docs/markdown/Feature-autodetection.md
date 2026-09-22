---
short-description: Auto-detection of features like ccache and code coverage
...

# Feature autodetection

Meson is designed for high productivity. It tries to do as many things
automatically as it possibly can.

Ccache
--

[Ccache](https://ccache.dev/) is a cache system designed to make
compiling faster. When you run Meson for the first time for a given
project, it checks if Ccache is installed. If it is, Meson will use it
automatically.

If you do not wish to use Ccache for some reason, just specify your
compiler with environment variables `CC` and/or `CXX` when first
running Meson (remember that once specified the compiler cannot be
changed). Meson will then use the specified compiler without Ccache.

Coverage
--

When doing a code coverage build, Meson will check for existence of
the binaries `gcovr`, `lcov` and `genhtml`. If version 3.3 or higher
of the first is found, targets called *coverage-text*, *coverage-xml*
and *coverage-html* are generated. If version 4.2 or higher of the
first is found, targets *coverage-text*, *coverage-xml*, *coverage-sonarqube*
and *coverage-html* are generated. Alternatively, if the latter two
are found, only the target *coverage-html* is generated. Coverage
reports can then be produced simply by calling e.g. `meson compile
coverage-xml`. As a convenience, a high-level *coverage* target is
also generated which will produce all 3 coverage report types, if
possible.

Note that generating any of the coverage reports described above
requires the tests (i.e. `meson test`) to finish running so the
information about the functions that are called in the tests can be
gathered for the report.
