---
short-description: Contributing to Meson
...

# Contributing to Meson

A large fraction of Meson is contributed by people outside the core
team. This documentation explains some of the design rationales of
Meson as well as how to create and submit your patches for inclusion
to Meson.

Thank you for your interest in participating to the development.

## Submitting patches

All changes must be submitted as [pull requests to
GitHub](https://github.com/mesonbuild/meson/pulls). This causes them
to be run through the CI system. All submissions must pass a full CI
test run before they are even considered for submission.

## Keeping pull requests up to date

It is possible that while your pull request is being reviewed, other
changes are committed to master that cause merge conflicts that must
be resolved. The basic rule for this is very simple: keep your pull
request up to date using rebase _only_.

Do not merge head back to your branch. Any merge commits in your pull
request make it not acceptable for merging into master and you must
remove them.

## Special procedure for new features

Every new feature requires some extra steps, namely:

- Must include a project test under `test cases/`, or if that's not
  possible or if the test requires a special environment, it must go
  into `run_unittests.py`.
- Must be registered with the [FeatureChecks
  framework](Release-notes-for-0.47.0.md#feature-detection-based-on-meson_version-in-project)
  that will warn the user if they try to use a new feature while
  targeting an older Meson version.
- Needs a release note snippet inside `docs/markdown/snippets/` with
  a heading and a brief paragraph explaining what the feature does
  with an example.

## Acceptance and merging

The kind of review and acceptance any merge proposal gets depends on
the changes it contains. All pull requests must be reviewed and
accepted by someone with commit rights who is not the original
submitter. Merge requests can be roughly split into three different
categories.

The first one consists of MRs that only change the markdown
documentation under `docs/markdown`. Anyone with access rights can
push changes to these directly to master. For major changes it is
still recommended to create a MR so other people can comment on it.

The second group consists of merges that don't change any
functionality, fixes to the CI system and bug fixes that have added
regression tests (see below) and don't change existing
functionality. Once successfully reviewed anyone with merge rights can
merge these to master.

The final kind of merges are those that add new functionality or
change existing functionality in a backwards incompatible way. These
require the approval of the project lead.

In a simplified list form the split would look like the following:

- members with commit access can do:
  - documentation changes (directly to master if warranted)
  - bug fixes that don't change functionality
  - refactorings
  - new dependency types
  - new tool support (e.g. a new Doxygen-kind of tool)
  - support for new compilers to existing languages
- project leader decision is needed for:
  - new modules
  - new functions in the Meson language
  - syntax changes for Meson files
  - changes breaking backwards compatibility
  - support for new languages

## A green CI run is mandatory for merging

No merge request may be merged until it has a fully green CI run. It
does not matter why CI fails, it is a hard blocker. Even if the MR
could possibly not have anything to do with the failure and clearly
should be permitted, it may not be merged. Only MRs that fix the CI
issue are allowed to land in trunk.

There is one, and only one, exception to this. At the time of writing
the Apple CI is unreliable and sometimes fails with clock skew errors.

If a merge causes CI failure any developer can revert it out of
master. It is then the responsibility of the original submitter to
resubmit a fixed version.

## Strategy for merging pull requests to trunk

Meson's merge strategy should fulfill the following guidelines:

- preserve as much history as possible

- have as little junk in the repo as possible

- everything in the "master lineage" should always pass all tests

These goals are slightly contradictory so the correct thing to do
often requires some judgement on part of the person doing the
merge. GitHub provides three different merge options, The rules of
thumb for choosing between them goes like this:

- single commit pull requests should always be rebased

- a pull request with one commit and one "fixup" commit (such as
  testing something to see if it passes CI) should be squashed

- large branches with many commits should be merged with a merge
  commit, especially if one of the commits does not pass all tests
  (which happens in e.g. large and difficult refactorings)

If in doubt, ask for guidance on IRC.

## Tests

All new features must come with automatic tests that thoroughly prove
that the feature is working as expected. Similarly bug fixes must come
with a unit test that demonstrates the bug, proves that it has been
fixed and prevents the feature from breaking in the future.

Sometimes it is difficult to create a unit test for a given bug. If
this is the case, note this in your pull request. We may permit bug
fix merge requests in these cases. This is done on a case by case
basis. Sometimes it may be easier to write the test than convince the
maintainers that one is not needed. Exercise judgment and ask for help
in problematic cases.

The tests are split into two different parts: unit tests and full
project tests. To run all tests, execute `./run_tests.py`. Unit tests
can be run with `./run_unittests.py` and project tests with
`./run_project_tests.py`.

### Project tests

Subsets of project tests can be selected with
`./run_project_tests.py --only` option. This can save a great deal of
time when only a certain part of Meson is being tested.
For example, a useful and easy contribution to Meson is making
sure the full set of compilers is supported. One could for example test
various Fortran compilers by setting `FC=ifort` or `FC=flang` or similar
with `./run_project_test.py --only fortran`.
Some families of tests require a particular backend to run.
For example, all the CUDA project tests run and pass on Windows via
`./run_project_tests.py --only cuda --backend ninja`

Each project test is a standalone project that can be compiled on its
own. They are all in the `test cases` subdirectory. The simplest way to
run a single project test is to do something like `./meson.py test\
cases/common/1\ trivial builddir`. The one exception to this is `test
cases/unit` directory discussed below.

The test cases in the `common` subdirectory are meant to be run always
for all backends. They should only depend on C and C++, without any
external dependencies such as libraries. Tests that require those are
in the `test cases/frameworks` directory. If there is a need for an
external program in the common directory, such as a code generator, it
should be implemented as a Python script. The goal of test projects is
also to provide sample projects that end users can use as a base for
their own projects.

All project tests follow the same pattern: they are configured,
compiled, tests are run and finally install is run. Passing means that
configuring, building and tests succeed and that installed files match
those expected.

Any tests that require more thorough analysis, such as checking that
certain compiler arguments can be found in the command line or that
the generated pkg-config files actually work should be done with a
unit test.

Additionally:

* `crossfile.ini` and `nativefile.ini` are passed to the configure step with
`--cross-file` and `--native-file` options, respectively.

* `mlog.cmd_ci_include()` can be called from anywhere inside Meson to
capture the contents of an additional file into the CI log on failure.

Projects needed by unit tests are in the `test cases/unit`
subdirectory. They are not run as part of `./run_project_tests.py`.

### Configuring project tests

The (optional) `test.json` file, in the root of a test case, is used
for configuring the test. All of the following root entries in the `test.json`
are independent of each other and can be combined as needed.

Example `test.json`:

```json
{
  "env": {
    "VAR": "VAL"
  },
  "installed": [
    { "type": "exe", "file": "usr/bin/testexe" },
    { "type": "pdb", "file": "usr/bin/testexe" },
    { "type": "shared_lib", "file": "usr/lib/z", "version": "1.2.3" },
  ],
  "matrix": {
    "options": {
      "opt1": [
        { "val": "abc"   },
        { "val": "qwert" },
        { "val": "bad"   }
      ],
      "opt2": [
        { "val": null    },
        { "val": "true"  },
        { "val": "false" },
      ]
    },
    "exclude": [
      { "opt1": "qwert", "opt2": "false" },
      { "opt1": "bad"                    }
    ]
  },
  "tools": {
    "cmake": ">=3.11"
  }
}
```

#### env

The `env` key contains a dictionary which specifies additional
environment variables to be set during the configure step of the test.

There is some basic support for configuring the string with the `@<VAR>@` syntax:

- `@ROOT@`: absolute path of the source directory
- `@PATH@`: current value of the `PATH` env variable

#### installed

The `installed` dict contains a list of dicts, describing which files are expected
to be installed. Each dict contains the following keys:

- `file`
- `type`
- `platform` (optional)
- `version` (optional)
- `language` (optional)

The `file` entry contains the relative path (from the install root) to the
actually installed file.

The `type` entry specifies how the `file` path should be interpreted based on the
current platform. The following values are currently supported:

| type          | Description                                                                                             |
| ------------- | ------------------------------------------------------------------------------------------------------- |
| `file`        | No postprocessing, just use the provided path                                                           |
| `python_file` | Use the provided path while replacing the python directory.                                             |
| `dir`         | To include all files inside the directory (for generated docs, etc). The path must be a valid directory |
| `exe`         | For executables. On Windows the `.exe` suffix is added to the path in `file`                            |
| `shared_lib`  | For shared libraries, always written as `name`. The appropriate suffix and prefix are added by platform |
| `python_lib`  | For python libraries, while replacing the python directory. The appropriate suffix is added by platform |
| `pdb`         | For Windows PDB files. PDB entries are ignored on non Windows platforms                                 |
| `implib`      | For Windows import libraries. These entries are ignored on non Windows platforms                        |
| `py_implib`   | For Windows import libraries. These entries are ignored on non Windows platforms                        |
| `implibempty` | Like `implib`, but no symbols are exported in the library                                               |
| `expr`        | `file` is an expression. This type should be avoided and removed if possible                            |

Except for the `file`, `python_file` and `expr` types, all paths should be provided *without* a suffix.

| Argument   | Applies to                 | Description                                                                   |
| -----------|----------------------------|-------------------------------------------------------------------------------|
| `version`  | `shared_lib`, `pdb`        | Sets the version to look for appropriately per-platform                       |
| `language` | `pdb`                      | Determines which compiler/linker determines the existence of this file        |

The `shared_lib` and `pdb` types takes an optional additional
parameter, `version`, this is us a string in `X.Y.Z` format that will
be applied to the library. Each version to be tested must have a
single version. The harness will apply this correctly per platform:

The `python_file`, `python_lib`, and `py_implib` types have basic support for configuring the string with the `@<VAR>@` syntax:

- `@PYTHON_PLATLIB@`: python `get_install_dir` directory relative to prefix
- `@PYTHON_PURELIB@`: python `get_install_dir(pure: true)` directory relative to prefix

`pdb` takes an optional `language` argument. This determines which
compiler/linker should generate the pdb file. Because it's possible to
mix compilers that do and don't generate pdb files (dmd's optlink
doesn't). Currently this is only needed when mixing D and C code.

```json
{
  "type": "shared_lib", "file": "usr/lib/lib",
  "type": "shared_lib", "file": "usr/lib/lib", "version": "1",
  "type": "shared_lib", "file": "usr/lib/lib", "version": "1.2.3.",
}
```

This will be applied appropriately per platform. On windows this
expects `lib.dll` and `lib-1.dll`. on MacOS it expects `liblib.dylib`
and `liblib.1.dylib`. On other Unices it expects `liblib.so`,
`liblib.so.1`, and `liblib.so.1.2.3`.

If the `platform` key is present, the installed file entry is only
considered if the platform matches. The following values for
`platform` are currently supported:

| platform   | Description                                                          |
| ---------- | -------------------------------------------------------------------- |
| `msvc`     | Matches when a msvc like compiler is used (`msvc`, `clang-cl`, etc.) |
| `gcc`      | Not `msvc`                                                           |
| `cygwin`   | Matches when the platform is cygwin                                  |
| `!cygwin`  | Not `cygwin`                                                         |

#### matrix

The `matrix` section can be used to define a test matrix to run
project tests with different Meson options.

In the `options` dict, all possible options and their values are
specified. Each key in the `options` dict is a Meson option. It stores
a list of all potential values in a dict format.

Each value must contain the `val` key for the value of the option.
`null` can be used for adding matrix entries without the current
option.

The `skip_on_env` key (as described below) may be used in the value to skip that
matrix entry, based on the current environment.

The `expect_skip_on_jobname` and `expect_skip_on_os` keys (as described below)
may be used to expect that the test will be skipped, based on the current environment.

Similarly, the `compilers` key can be used to define a mapping of
compilers to languages that are required for this value.

```json
{
  "compilers": {
    "c": "gcc",
    "cpp": "gcc",
    "d": "gdc"
  }
}
```

Specific option combinations can be excluded with the `exclude`
section. It should be noted that `exclude` does not require exact
matches. Instead, any matrix entry containing all option value
combinations in `exclude` will be excluded. Thus an empty dict (`{}`)
to will match **all** elements in the test matrix.

The above example will produce the following matrix entries:
- `opt1=abc`
- `opt1=abc opt2=true`
- `opt1=abc opt2=false`
- `opt1=qwert`
- `opt1=qwert opt2=true`

#### do_not_set_opts

Currently supported values are:
- `prefix`
- `libdir`

#### tools

This section specifies a dict of tool requirements in a simple
key-value format. If a tool is specified, it has to be present in the
environment, and the version requirement must be fulfilled. Otherwise,
the entire test is skipped (including every element in the test
matrix).

#### stdout

The `stdout` key contains a list of dicts, describing the expected
stdout.

Each dict contains the following keys:

- `line`
- `match` (optional)
- `count` (optional)

Each item in the list is matched, in order, against the remaining
actual stdout lines, after any previous matches. If the actual stdout
is exhausted before every item in the list is matched, the expected
output has not been seen, and the test has failed.

The `match` element of the dict determines how the `line` element is
matched:

| Type      | Description             |
| --------  | ----------------------- |
| `literal` | Literal match (default) |
| `re`      | regex match             |

The `count` element determines how many times the line is expected, and allowed,
to be in the output. If unspecified, it must appear "any number of times, but at
least once".

#### skip_on_env

The `skip_on_env` key can be used to specify a list of environment variables. If
at least one environment variable in the `skip_on_env` list is present, the test
is skipped.

#### expect_skip_on_jobname

The `expect_skip_on_jobname` key contains a list of strings. If the `MESON_CI_JOBNAME`
environment variable is set, and any of them are a sub-string of it, the test is
expected to be skipped (that is, it is expected that the test will output
`MESON_SKIP_TEST`, because the CI environment is not one in which it can run,
for whatever reason).

The test is failed if it either skips unexpectedly or runs unexpectedly.

#### expect_skip_on_os

The `expect_skip_on_os` key can be used to specify a list of OS names (or their
negations, prefixed with a `!`).  If at least one item in the `expect_skip_on_os` list
is matched, the test is expected to be skipped.

The test is failed if it either skips unexpectedly or runs unexpectedly.

### Skipping integration tests

Meson uses several continuous integration testing systems that have
slightly different interfaces for indicating a commit should be
skipped.

Continuous integration systems currently used:
- [Azure Pipelines](https://docs.microsoft.com/en-us/azure/devops/pipelines/scripts/git-commands?view=vsts&tabs=yaml#how-do-i-avoid-triggering-a-ci-build-when-the-script-pushes)
  allows `***NO_CI***` in the commit message.
- [Sider](https://sider.review)
  runs Flake8 ([see below](#python-coding-style))

To promote consistent naming policy, use:

- `[skip ci]` in the commit title if you want to disable all
  integration tests

## Documentation

The `docs` directory contains the full documentation that will be used
to generate [the Meson web site](https://mesonbuild.com). Line length
in most cases should not exceed 70 characters (lines containing links
or examples are usually exempt). Every change in functionality must
change the documentation pages. In most cases this means updating the
reference documentation page but bigger changes might need changes in
other documentation, too.

All new functionality needs to have a mention in the release
notes. These features should be written in standalone files in the
`docs/markdown/snippets` directory. The release manager will combine
them into one page when doing the release.

## Python Coding style

Meson follows the basic Python coding style. Additional rules are the
following:

- indent 4 spaces, no tabs ever
- indent meson.build files with two spaces
- try to keep the code as simple as possible
- contact the mailing list before embarking on large scale projects
  to avoid wasted effort

Meson uses Flake8 for style guide enforcement. The Flake8 options for
the project are contained in .flake8.

To run Flake8 on your local clone of Meson:

```console
$ python3 -m pip install flake8
$ cd meson
$ flake8
```

To run it automatically before committing:

```console
$ flake8 --install-hook=git
$ git config --bool flake8.strict true
```

## C/C++ coding style

Meson has a bunch of test code in several languages. The rules for
those are simple.

- indent 4 spaces, no tabs ever
- brace always on the same line as if/for/else/function definition

## External dependencies

The goal of Meson is to be as easily usable as possible. The user
experience should be "get Python3 and Ninja, run", even on
Windows. Unfortunately this means that we can't have dependencies on
projects outside of Python's standard library. This applies only to
core functionality, though. For additional helper programs etc the use
of external dependencies may be ok. If you feel that you are dealing
with this kind of case, please contact the developers first with your
use case.

## Turing completeness

The main design principle of Meson is that the definition language is
not Turing complete. Any change that would make Meson Turing complete
is automatically rejected. In practice this means that defining your
own functions inside `meson.build` files and generalised loops will
not be added to the language.

## Do I need to sign a CLA in order to contribute?

No you don't. All contributions are welcome.

## No lingering state

Meson operates in much the same way as functional programming
languages. It has inputs, which include `meson.build` files, values of
options, compilers and so on. These are passed to a function, which
generates output build definition. This function is pure, which means that:

- for any given input the output is always the same
- running Meson twice in a row _always_ produce the same output in both runs

The latter one is important, because it enforces that there is no way
for "secret state" to pass between consecutive invocations of
Meson. This is the reason why, for example, there is no `set_option`
function even though there is a `get_option` one.

If this were not the case, we could never know if the build output is
"stable". For example suppose there were a `set_option` function and a
boolean variable `flipflop`. Then you could do this:

```meson
set_option('flipflop', not get_option('flipflop'))
```

This piece of code would never converge. Every Meson run would change
the value of the option and thus the output you get out of this build
definition would be random.

Meson does not permit this by forbidding these sorts of covert
channels.

There is one exception to this rule. Users can call into external
commands with `run_command`. If the output of that command does not
behave like a pure function, this problem arises. Meson does not try
to guard against this case, it is the responsibility of the user to
make sure the commands they run behave like pure functions.

## Environment variables

Environment variables are like global variables, except that they are
also hidden by default. Envvars should be avoided whenever possible,
all functionality should be exposed in better ways such as command
line switches.

## Random design points that fit nowhere else

- All features should follow the 90/9/1 rule. 90% of all use cases
  should be easy, 9% should be possible and it is totally fine to not
  support the final 1% if it would make things too complicated.

- Any build directory will have at most two toolchains: one native and
  one cross.

- Prefer specific solutions to generic frameworks. Solve the end
  user's problems rather than providing them tools to do it
  themselves.

- Never use features of the Unix shell (or Windows shell for that
  matter). Doing things like forwarding output with `>` or invoking
  multiple commands with `&&` are not permitted. Whenever these sorts
  of requirements show up, write an internal Python script with the
  desired functionality and use that instead.
