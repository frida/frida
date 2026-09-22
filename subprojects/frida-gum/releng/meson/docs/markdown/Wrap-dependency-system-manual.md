# Wrap dependency system manual

One of the major problems of multiplatform development is wrangling
all your dependencies. This is awkward on many platforms, especially
on ones that do not have a built-in package manager. The latter problem
has been worked around by having third party package managers. They
are not really a solution for end user deployment, because you can't
tell them to install a package manager just to use your app. On these
platforms you must produce self-contained applications. Same applies
when destination platform is missing (up-to-date versions of) your
application's dependencies.

The traditional approach to this has been to bundle dependencies
inside your own project. Either as prebuilt libraries and headers or
by embedding the source code inside your source tree and rewriting
your build system to build them as part of your project.

This is both tedious and error prone because it is always done by
hand. The Wrap dependency system of Meson aims to provide an automated
way to do this.

## How it works

Meson has a concept of [subprojects](Subprojects.md). They are a way
of nesting one Meson project inside another. Any project that builds
with Meson can detect that it is built as a subproject and build
itself in a way that makes it easy to use (usually this means as a
static library).

To use this kind of a project as a dependency you could just copy and
extract it inside your project's `subprojects` directory.

However there is a simpler way. You can specify a Wrap file that tells
Meson how to download it for you. If you then use this subproject in
your build, Meson will automatically download and extract it during
build. This makes subproject embedding extremely easy.

All wrap files must have a name of `<project_name>.wrap` form and be
in `subprojects` dir.

Currently Meson has four kinds of wraps:
- wrap-file
- wrap-git
- wrap-hg
- wrap-svn

## wrap format

Wrap files are written in ini format, with a single header containing
the type of wrap, followed by properties describing how to obtain the
sources, validate them, and modify them if needed. An example
wrap-file for the wrap named `libfoobar` would have a filename
`libfoobar.wrap` and would look like this:

```ini
[wrap-file]
directory = libfoobar-1.0

source_url = https://example.com/foobar-1.0.tar.gz
source_filename = foobar-1.0.tar.gz
source_hash = 5ebeea0dfb75d090ea0e7ff84799b2a7a1550db3fe61eb5f6f61c2e971e57663
```

An example wrap-git will look like this:

```ini
[wrap-git]
url = https://github.com/libfoobar/libfoobar.git
revision = head
depth = 1
```

## Accepted configuration properties for wraps

- `directory` - name of the subproject root directory, defaults to the
  name of the wrap.

Since *0.55.0* those can be used in all wrap types, they were
previously reserved to `wrap-file`:

- `patch_url` - download url to retrieve an optional overlay archive
- `patch_fallback_url` - fallback URL to be used when download from `patch_url` fails *Since: 0.55.0*
- `patch_filename` - filename of the downloaded overlay archive
- `patch_hash` - sha256 checksum of the downloaded overlay archive
- `patch_directory` - *Since 0.55.0* Overlay directory, alternative to `patch_filename` in the case
  files are local instead of a downloaded archive. The directory must be placed in
  `subprojects/packagefiles`.
- `diff_files` - *Since 0.63.0* Comma-separated list of local diff files (see
  [Diff files](#diff-files) below).
- `method` - *Since 1.3.0* The build system used by this subproject. Defaults to `meson`.
  Supported methods:
  - `meson` requires `meson.build` file.
  - `cmake` requires `CMakeLists.txt` file. [See details](#cmake-wraps).
  - `cargo` requires `Cargo.toml` file. [See details](#cargo-wraps).

### Specific to wrap-file
- `source_url` - download url to retrieve the wrap-file source archive
- `source_fallback_url` - fallback URL to be used when download from `source_url` fails *Since: 0.55.0*
- `source_filename` - filename of the downloaded source archive
- `source_hash` - sha256 checksum of the downloaded source archive
- `lead_directory_missing` - for `wrap-file` create the leading
  directory name. Needed when the source file does not have a leading
  directory.

Since *0.55.0* it is possible to use only the `source_filename` and
`patch_filename` value in a .wrap file (without `source_url` and
`patch_url`) to specify a local archive in the
`subprojects/packagefiles` directory. The `*_hash` entries are
optional when using this method. This method should be preferred over
the old `packagecache` approach described below.

Since *0.49.0* if `source_filename` or `patch_filename` is found in the
project's `subprojects/packagecache` directory, it will be used instead
of downloading the file, even if `--wrap-mode` option is set to
`nodownload`. The file's hash will be checked.

Since *1.3.0* if the `MESON_PACKAGE_CACHE_DIR` environment variable is set, it is used instead of
the project's `subprojects/packagecache`. This allows sharing the cache across multiple
projects. In addition it can contain an already extracted source tree as long as it
has the same directory name as the `directory` field in the wrap file. In that
case, the directory will be copied into `subprojects/` before applying patches.

### Specific to VCS-based wraps
- `url` - name of the wrap-git repository to clone. Required.
- `revision` - name of the revision to checkout. Must be either: a
  valid value (such as a git tag) for the VCS's `checkout` command, or
  (for git) `head` to track upstream's default branch. Required.

### Specific to wrap-git
- `depth` - shallowly clone the repository to X number of commits. This saves bandwidth and disk
  space, and should typically always be specified unless commit history is needed. Note
  that git always allow shallowly cloning branches, but in order to
  clone commit ids shallowly, the server must support
  `uploadpack.allowReachableSHA1InWant=true`.  *(since 0.52.0)*
- `push-url` - alternative url to configure as a git push-url. Useful if
  the subproject will be developed and changes pushed upstream.
  *(since 0.37.0)*
- `clone-recursive` - also clone submodules of the repository
  *(since 0.48.0)*

## wrap-file with Meson build patch

Unfortunately most software projects in the world do not build with
Meson. Because of this Meson allows you to specify a patch URL.

For historic reasons this is called a "patch", however, it serves as an
overlay to add or replace files rather than modifying them. The file
must be an archive; it is downloaded and automatically extracted into
the subproject. The extracted files will include a Meson build
definition for the given subproject.

This approach makes it extremely simple to embed dependencies that
require build system changes. You can write the Meson build definition
for the dependency in total isolation. This is a lot better than doing
it inside your own source tree, especially if it contains hundreds of
thousands of lines of code. Once you have a working build definition,
just zip up the Meson build files (and others you have changed) and
put them somewhere where you can download them.

Prior to *0.55.0* Meson build patches were only supported for
wrap-file mode. When using wrap-git, the repository must contain all
Meson build definitions. Since *0.55.0* Meson build patches are
supported for any wrap modes, including wrap-git.

## Diff files

*Since: 0.63.0*

You can also provide local patch files in `diff` format. For historic reasons,
they are referred to as "diff files", since the "patch" name is already used for
overlay archives.

The diff files are described by the `diff_files` property (a comma-separated
list), and must be available locally in the `subprojects/packagefiles`
directory.

Meson will apply the diff files after extracting or cloning the project, and
after applying the overlay archive (`patch_*`). For this feature, the `patch` or
`git` command-line tool must be available.

The diff files will be applied with `-p1`, i.e. treating the first path
component as a prefix to be stripped. This is the default for diffs produced by
Git.

```ini
[wrap-file]
directory = libfoobar-1.0

source_url = https://example.com/foobar-1.0.tar.gz
source_filename = foobar-1.0.tar.gz
source_hash = 5ebeea0dfb75d090ea0e7ff84799b2a7a1550db3fe61eb5f6f61c2e971e57663

diff_files = libfoobar-1.0/0001.patch, libfoobar-1.0/0002.patch
```

## `provide` section

*Since *0.55.0*

Wrap files can define the dependencies it provides in the `[provide]`
section.

```ini
[provide]
dependency_names = foo-1.0
```

When a wrap file provides the dependency `foo-1.0`, as above, any call to
`dependency('foo-1.0')` will automatically fallback to that subproject even if
no `fallback` keyword argument is given. A wrap file named `foo.wrap` implicitly
provides the dependency name `foo` even when the `[provide]` section is missing.

Optional dependencies, like `dependency('foo-1.0', required: get_option('foo_opt'))`
where `foo_opt` is a feature option set to `auto`, will not fallback to the
subproject defined in the wrap file, for 2 reasons:
- It allows for looking the dependency in other ways first, for example using
  `cc.find_library('foo')`, and only fallback if that fails:

```meson
# this won't use fallback defined in foo.wrap
foo_dep = dependency('foo-1.0', required: false)
if not foo_dep.found()
  foo_dep = cc.find_library('foo', has_headers: 'foo.h', required: false)
  if not foo_dep.found()
    # This will use the fallback
    foo_dep = dependency('foo-1.0')
    # or
    foo_dep = dependency('foo-1.0', required: false, fallback: 'foo')
  endif
endif
```

- Sometimes not-found dependency is preferable to a fallback when the
  feature is not explicitly requested by the user. In that case
  `dependency('foo-1.0', required: get_option('foo_opt'))` will only
  fallback when the user sets `foo_opt` to `enabled` instead of
  `auto`.
*Since 0.58.0* optional dependency like above will fallback to the subproject
defined in the wrap file in the case `wrap_mode` is set to `forcefallback`
or `force_fallback_for` contains the subproject.

If it is desired to fallback for an optional dependency, the
`fallback` or `allow_fallback` keyword arguments must be passed
explicitly. *Since 0.56.0*, `dependency('foo-1.0', required:
get_option('foo_opt'), allow_fallback: true)` will use the fallback
even when `foo_opt` is set to `auto`. On version *0.55.0* the same
effect could be achieved with `dependency('foo-1.0', required:
get_option('foo_opt'), fallback: 'foo')`.

This mechanism assumes the subproject calls
`meson.override_dependency('foo-1.0', foo_dep)` so Meson knows which
dependency object should be used as fallback. Since that method was
introduced in version *0.54.0*, as a transitional aid for projects
that do not yet make use of it the variable name can be provided in
the wrap file with entries in the format `foo-1.0 = foo_dep`.

For example when using a recent enough version of glib that uses
`meson.override_dependency()` to override `glib-2.0`, `gobject-2.0`
and `gio-2.0`, a wrap file would look like:

```ini
[wrap-git]
url=https://gitlab.gnome.org/GNOME/glib.git
revision=glib-2-62
depth=1

[provide]
dependency_names = glib-2.0, gobject-2.0, gio-2.0
```

With older version of glib dependency variable names need to be
specified:

```ini
[wrap-git]
url=https://gitlab.gnome.org/GNOME/glib.git
revision=glib-2-62
depth=1

[provide]
glib-2.0=glib_dep
gobject-2.0=gobject_dep
gio-2.0=gio_dep
```

Programs can also be provided by wrap files, with the `program_names`
key:

```ini
[provide]
program_names = myprog, otherprog
```

With such wrap file, `find_program('myprog')` will automatically
fallback to use the subproject, assuming it uses
`meson.override_find_program('myprog')`.

### CMake wraps

Since the CMake module does not know the public name of the provided
dependencies, a CMake `.wrap` file cannot use the `dependency_names = foo`
syntax. Instead, the `dep_name = <target_name>_dep` syntax should be used, where
`<target_name>` is the name of a CMake library with all non alphanumeric
characters replaced by underscores `_`.

For example, a CMake project that contains `add_library(foo-bar ...)` in its
`CMakeList.txt` and that applications would usually find using the dependency
name `foo-bar-1.0` (e.g. via pkg-config) would have a wrap file like this:

```ini
[wrap-file]
...
method = cmake
[provide]
foo-bar-1.0 = foo_bar_dep
```
### Cargo wraps

Cargo subprojects automatically override the `<package_name>-<version>-rs` dependency
name:
- `package_name` is defined in `[package] name = ...` section of the `Cargo.toml`.
- `version` is the API version deduced from `[package] version = ...` as follow:
  * `x.y.z` -> 'x'
  * `0.x.y` -> '0.x'
  * `0.0.x` -> '0'
  It allows to make different dependencies for uncompatible versions of the same
  crate.
- `-rs` suffix is added to distinguish from regular system dependencies, for
  example `gstreamer-1.0` is a system pkg-config dependency and `gstreamer-0.22-rs`
  is a Cargo dependency.

That means the `.wrap` file should have `dependency_names = foo-1-rs` in their
`[provide]` section when `Cargo.toml` has package name `foo` and version `1.2`.

Note that the version component was added in Meson 1.4, previous versions were
using `<package_name>-rs` format.

Cargo subprojects require a toml parser. Python >= 3.11 have one built-in, older
Python versions require either the external `tomli` module or `toml2json` program.

For example, a Cargo project with the package name `foo-bar` would have a wrap
file like that:
```ini
[wrap-file]
...
method = cargo
[provide]
dependency_names = foo-bar-0.1-rs
```

Cargo features are exposed as Meson boolean options, with the `feature-` prefix.
For example the `default` feature is named `feature-default` and can be set from
the command line with `-Dfoo-1-rs:feature-default=false`. When a cargo subproject
depends on another cargo subproject, it will automatically enable features it
needs using the `dependency('foo-1-rs', default_options: ...)` mechanism. However,
unlike Cargo, the set of enabled features is not managed globally. Let's assume
the main project depends on `foo-1-rs` and `bar-1-rs`, and they both depend on
`common-1-rs`. The main project will first look up `foo-1-rs` which itself will
configure `common-rs` with a set of features. Later, when `bar-1-rs` does a lookup
for `common-1-rs` it has already been configured and the set of features cannot be
changed. If `bar-1-rs` wants extra features from `common-1-rs`, Meson will error out.
It is currently the responsability of the main project to resolve those
issues by enabling extra features on each subproject:
```meson
project(...,
  default_options: {
    'common-1-rs:feature-something': true,
  },
)
```

In addition, if the file `meson/meson.build` exists, Meson will call `subdir('meson')`
where the project can add manual logic that would usually be part of `build.rs`.
Some naming conventions need to be respected:
- The `extra_args` variable is pre-defined and can be used to add any Rust arguments.
  This is typically used as `extra_args += ['--cfg', 'foo']`.
- The `extra_deps` variable is pre-defined and can be used to add extra dependencies.
  This is typically used as `extra_deps += dependency('foo')`.

## Using wrapped projects

Wraps provide a convenient way of obtaining a project into your
subproject directory. Then you use it as a regular subproject (see
[subprojects](Subprojects.md)).

## Getting wraps

Usually you don't want to write your wraps by hand.

There is an online repository called
[WrapDB](https://wrapdb.mesonbuild.com) that provides many
dependencies ready to use. You can read more about WrapDB
[here](Using-the-WrapDB.md).

There is also a Meson subcommand to get and manage wraps (see [using
wraptool](Using-wraptool.md)).
