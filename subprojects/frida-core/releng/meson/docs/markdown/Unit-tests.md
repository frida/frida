---
short-description: Meson's own unit-test system
...

# Unit tests

Meson comes with a fully functional unit test system. To use it simply
build an executable and then use it in a test.

```meson
e = executable('prog', 'testprog.c')
test('name of test', e)
```

You can add as many tests as you want. They are run with the command `meson
test`.

Meson captures the output of all tests and writes it in the log file
`meson-logs/testlog.txt`.

## Test parameters

Some tests require the use of command line arguments or environment
variables. These are simple to define.

```meson
test('command line test', exe, args : ['first', 'second'])
test('envvar test', exe2, env : ['key1=value1', 'key2=value2'])
```

Note how you need to specify multiple values as an array.

### MALLOC_PERTURB_

By default, environment variable
[`MALLOC_PERTURB_`](http://man7.org/linux/man-pages/man3/mallopt.3.html) is
set to a random value between 1..255. This can help find memory leaks on
configurations using glibc, including with non-GCC compilers. This feature
can be disabled as discussed in [[test]].

### ASAN_OPTIONS, UBSAN_OPTIONS, and MSAN_OPTIONS

By default, the environment variables `ASAN_OPTIONS`, `UBSAN_OPTIONS`, and
`MSAN_OPTIONS` are set to enable aborting on detected violations and to give a
backtrace. This feature can be disabled as discussed in [[test]].

## Coverage

If you enable coverage measurements by giving Meson the command line
flag `-Db_coverage=true`, you can generate coverage reports after
running the tests (running the tests is required to gather the list of
functions that get called). Meson will autodetect what coverage
generator tools you have installed and will generate the corresponding
targets. These targets are `coverage-xml` and `coverage-text` which
are both provided by [Gcovr](http://gcovr.com) (version 3.3 or higher)
`coverage-sonarqube` which is provided by [Gcovr](http://gcovr.com) (version 4.2 or higher)
and `coverage-html`, which requires
[lcov](https://github.com/linux-test-project/lcov) and
[GenHTML](https://linux.die.net/man/1/genhtml) or
[Gcovr](http://gcovr.com). As a convenience, a high-level `coverage`
target is also generated which will produce all 3 coverage report
types, if possible.

The output of these commands is written to the log directory `meson-logs` in
your build directory.

## Parallelism

To reduce test times, Meson will by default run multiple unit tests in
parallel. It is common to have some tests which cannot be run in
parallel because they require unique hold on some resource such as a
file or a D-Bus name. You have to specify these tests with a keyword
argument.

```meson
test('unique test', t, is_parallel : false)
```

Meson will then make sure that no other unit test is running at the
same time. Non-parallel tests take longer to run so it is recommended
that you write your unit tests to be parallel executable whenever
possible.

By default Meson uses as many concurrent processes as there are cores
on the test machine. You can override this with the environment
variable `MESON_TESTTHREADS` like this.

```console
$ MESON_TESTTHREADS=5 meson test
```

## Priorities

*(added in version 0.52.0)*

Tests can be assigned a priority that determines when a test is
*started*. Tests with higher priority are started first, tests with
lower priority started later. The default priority is 0, Meson makes
no guarantee on the ordering of tests with identical priority.

```meson
test('started second', t, priority : 0)
test('started third', t, priority : -50)
test('started first', t, priority : 1000)
```

Note that the test priority only affects the starting order of tests
and subsequent tests are affected by how long it takes previous tests
to complete. It is thus possible that a higher-priority test is still
running when lower-priority tests with a shorter runtime have
completed.

## Skipped tests and hard errors

Sometimes a test can only determine at runtime that it cannot be run.

For the default `exitcode` testing protocol, the GNU standard approach
in this case is to exit the program with error code 77. Meson will
detect this and report these tests as skipped rather than failed. This
behavior was added in version 0.37.0.

For TAP-based tests, skipped tests should print a single line starting
with `1..0 # SKIP`.

In addition, sometimes a test fails set up so that it should fail even
if it is marked as an expected failure. The GNU standard approach in
this case is to exit the program with error code 99. Again, Meson will
detect this and report these tests as `ERROR`, ignoring the setting of
`should_fail`. This behavior was added in version 0.50.0.

## Testing tool

The goal of the Meson test tool is to provide a simple way to run
tests in a variety of different ways. The tool is designed to be run
in the build directory.

The simplest thing to do is just to run all tests.

```console
$ meson test
```

### Run subsets of tests

For clarity, consider the meson.build containing:

```meson

test('A', ..., suite: 'foo')
test('B', ..., suite: ['foo', 'bar'])
test('C', ..., suite: 'bar')
test('D', ..., suite: 'baz')

```

Specify test(s) by name like:

```console
$ meson test A D
```

You can run tests from specific (sub)project:

```console
$ meson test (sub)project_name:
```

or a specific test in a specific project:

```console
$ meson test (sub)project_name:test_name
```

Since version *1.2.0*, you can use wildcards in project
and test names. For instance, to run all tests beginning with
"foo" and all tests from projects beginning with "bar":

```console
$ meson test "foo*" "bar*:"
```


Tests belonging to a suite `suite` can be run as follows

```console
$ meson test --suite (sub)project_name:suite
```

Since version *0.46*, `(sub)project_name` can be omitted if it is the
top-level project.

Multiple suites are specified like:

```console
$ meson test --suite foo --suite bar
```

NOTE: If you choose to specify both suite(s) and specific test
name(s), the test name(s) must be contained in the suite(s). This
however is redundant-- it would be more useful to specify either
specific test names or suite(s).

### Other test options

Sometimes you need to run the tests multiple times, which is done like this:

```console
$ meson test --repeat=10
```

Invoking tests via a helper executable such as Valgrind can be done with the
`--wrap` argument

```console
$ meson test --wrap=valgrind testname
```

Arguments to the wrapper binary can be given like this:

```console
$ meson test --wrap='valgrind --tool=helgrind' testname
```

Meson also supports running the tests under GDB. Just doing this:

```console
$ meson test --gdb testname
```

Meson will launch `gdb` all set up to run the test. Just type `run` in
the GDB command prompt to start the program.

The second use case is a test that segfaults only rarely. In this case
you can invoke the following command:

```console
$ meson test --gdb --repeat=10000 testname
```

This runs the test up to 10 000 times under GDB automatically. If the
program crashes, GDB will halt and the user can debug the application.
Note that testing timeouts are disabled in this case so `meson test`
will not kill `gdb` while the developer is still debugging it. The
downside is that if the test binary freezes, the test runner will wait
forever.

Sometimes, the GDB binary is not in the PATH variable or the user
wants to use a GDB replacement. Therefore, the invoked GDB program can
be specified *(added 0.52.0)*:

```console
$ meson test --gdb --gdb-path /path/to/gdb testname
```

```console
$ meson test --print-errorlogs
```

Meson will report the output produced by the failing tests along with
other useful information as the environmental variables. This is
useful, for example, when you run the tests on Travis-CI, Jenkins and
the like.

**Timeout**

In the test case options, the `timeout` option is specified in a number of seconds.

To disable timeout in test cases, add `timeout: 0` or a negative value to allow
infinite duration for the test case to complete.

For running tests, you can specify a command line argument for overriding the
timeout as well:

```console
$ meson test --timeout-multiplier 0
```

For further information see the command line help of Meson by running
`meson test -h`.

## Legacy notes

If `meson test` does not work for you, you likely have a old version
of Meson. In that case you should call `mesontest` instead. If
`mesontest` doesn't work either you have a very old version prior to
0.37.0 and should upgrade.

## Test outputs

Meson will write several different files with detailed results of
running tests. These will be written into $builddir/meson-logs/

### testlog.json

This is not a proper json file, but a file containing one valid json
object per line. This is file is designed so each line is streamed out
as each test is run, so it can be read as a stream while the test
harness is running

### testlog.junit.xml

This is a valid JUnit XML description of all tests run. It is not
streamed out, and is written only once all tests complete running.

When tests use the `tap` protocol each test will be recorded as a
testsuite container, with each case named by the number of the result.

When tests use the `gtest` protocol Meson will inject arguments to the
test to generate its own JUnit XML, which Meson will include as part
of this XML file.

*New in 0.55.0*
