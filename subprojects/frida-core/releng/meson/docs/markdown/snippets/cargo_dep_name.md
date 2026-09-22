## Cargo dependencies names now include the API version

Cargo dependencies names are now in the format `<package_name>-<version>-rs`:
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

This is a breaking change (Cargo subprojects are still experimental), previous
versions were using `<package_name>-rs` format.
