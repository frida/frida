# Windows module

This module provides functionality used to build applications for
Windows.

## Methods

### compile_resources

```
  windows = import('windows')
  windows.compile_resources(...(string | File | CustomTarget | CustomTargetIndex),
                            args: []string,
                            depend_files: [](string | File),
                            depends: [](BuildTarget | CustomTarget)
                            include_directories: [](IncludeDirectories | string)): []CustomTarget
```

Compiles Windows `rc` files specified in the positional arguments.
Returns a list of `CustomTarget` objects that you put in the list of sources for
the target you want to have the resources in.

*Since 0.61.0* CustomTargetIndexes and CustomTargets with more than out output
*may be used as positional arguments.

This method has the following keyword arguments:

- `args` lists extra arguments to pass to the resource compiler
- `depend_files` lists resource files that the resource script depends on
  (e.g. bitmap, cursor, font, html, icon, message table, binary data or manifest
  files referenced by the resource script) (*since 0.47.0*)
- `depends` lists target(s) that this target depends on, even though it does not
  take them as an argument (e.g. as above, but generated) (*since 0.47.0*)
- `include_directories` lists directories to be both searched by the resource
  compiler for referenced resource files, and added to the preprocessor include
  search path.

The resource compiler executable used is the first which exists from the
following list:

1. The `windres` executable given in the `[binaries]` section of the cross-file
2. The `WINDRES` environment variable
3. The resource compiler which is part of the same toolset as the C or C++ compiler in use.
