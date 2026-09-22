# FS (filesystem) module

This module provides functions to inspect the file system. It is
available starting with version 0.53.0.

Since 0.59.0, all functions accept `files()` objects if they can do something
useful with them (this excludes `exists`, `is_dir`, `is_file`, `is_absolute`
since a `files()` object is always the absolute path to an existing file).

## Usage

The module may be imported as follows:

``` meson
fs = [[#import]]('fs')
```

## File lookup rules

Non-absolute paths are looked up relative to the directory where the
current `meson.build` file is.

If specified, a leading `~` is expanded to the user home directory.
Environment variables are not available as is the rule throughout Meson.
That is, $HOME, %USERPROFILE%, $MKLROOT, etc. have no meaning to the Meson
filesystem module. If needed, pass such variables into Meson via command
line options in `meson.options`, native-file or cross-file.

Where possible, symlinks and parent directory notation are resolved to an
absolute path.

### exists

Takes a single string argument and returns true if an entity with that
name exists on the file system. This can be a file, directory or a
special entry such as a device node.

### is_dir

Takes a single string argument and returns true if a directory with
that name exists on the file system.

### is_file

Takes a single string argument and returns true if an file with that
name exists on the file system.

### is_symlink

Takes a single string or (since 0.59.0) `files()` argument and returns true if
the path pointed to by the string is a symbolic link.

## File Parameters

### is_absolute

*since 0.54.0*

Return a boolean indicating if the path string or (since 0.59.0) `files()`
specified is absolute, WITHOUT expanding `~`.

Examples:

```meson
fs.is_absolute('~')   # false

home = fs.expanduser('~')
fs.is_absolute(home)  # true

fs.is_absolute(home / 'foo')  # true, even if ~/foo doesn't exist

fs.is_absolute('foo/bar')  # false, even if ./foo/bar exists
```

### hash

The `fs.hash(filename, hash_algorithm)` method returns a string containing
the hexadecimal `hash_algorithm` digest of a file.
`hash_algorithm` is a string; the available hash algorithms include:
md5, sha1, sha224, sha256, sha384, sha512.

### size

The `fs.size(filename)` method returns the size of the file in integer bytes.

### is_samepath

The `fs.is_samepath(path1, path2)` returns boolean `true` if both
paths resolve to the same path. For example, suppose path1 is a
symlink and path2 is a relative path. If `path1` can be resolved to
`path2`, then `true` is returned. If `path1` is not resolved to
`path2`, `false` is returned. If `path1` or `path2` do not exist,
`false` is returned.

Examples:

```meson
x = 'foo.txt'
y = 'sub/../foo.txt'
z = 'bar.txt'  # a symlink pointing to foo.txt
j = 'notafile.txt'  # nonexistent file

fs.is_samepath(x, y)  # true
fs.is_samepath(x, z)  # true
fs.is_samepath(x, j)  # false

p = 'foo/bar'
q = 'foo/bar/baz/..'
r = 'buz'  # a symlink pointing to foo/bar
s = 'notapath'  # nonexistent directory

fs.is_samepath(p, q)  # true
fs.is_samepath(p, r)  # true
fs.is_samepath(p, s)  # false
```

## Filename modification

The files need not actually exist yet for these path string
manipulation methods.

### expanduser

*since 0.54.0*

A path string with a leading `~` is expanded to the user home
directory

Examples:

```meson
fs.expanduser('~')  # user home directory

fs.expanduser('~/foo')  # <homedir>/foo
```

### as_posix

*since 0.54.0*

`fs.as_posix(path)` assumes a Windows path, even if on a Unix-like
system. Thus, all `'\'` or `'\\'` are turned to '/', even if you meant
to escape a character.

Examples

```meson
fs.as_posix('\\') == '/'  # true
fs.as_posix('\\\\') == '/'  # true

fs.as_posix('foo\\bar/baz') == 'foo/bar/baz'  # true
```

### replace_suffix

The `replace_suffix` method is a *string manipulation* convenient for
filename modifications. It allows changing the filename suffix like:

#### swap suffix

```meson
original = '/opt/foo.ini'
new = fs.replace_suffix(original, '.txt')  # /opt/foo.txt
```

#### add suffix

```meson
original = '/opt/foo'
new = fs.replace_suffix(original, '.txt')  # /opt/foo.txt
```

#### compound suffix swap

```meson
original = '/opt/foo.dll.a'
new = fs.replace_suffix(original, '.so')  # /opt/foo.dll.so
```

#### delete suffix

```meson
original = '/opt/foo.dll.a'
new = fs.replace_suffix(original, '')  # /opt/foo.dll
```

### parent

Returns the parent directory (i.e. dirname).

```meson
new = fs.parent('foo/bar')  # foo
new = fs.parent('foo/bar/baz.dll')  # foo/bar
```

### name

Returns the last component of the path (i.e. basename).

```meson
fs.name('foo/bar/baz.dll.a')  # baz.dll.a
```

### stem

*since 0.54.0*

Returns the last component of the path, dropping the last part of the
suffix

```meson
fs.stem('foo/bar/baz.dll')  # baz
fs.stem('foo/bar/baz.dll.a')  # baz.dll
```

### read
- `read(path, encoding: 'utf-8')` *(since 0.57.0)*:
   return a [string](Syntax.md#strings) with the contents of the given `path`.
   If the `encoding` keyword argument is not specified, the file specified by
   `path` is assumed to be utf-8 encoded. Binary files are not supported. The
   provided paths should be relative to the current `meson.current_source_dir()`
   or an absolute path outside the build directory is accepted. If the file
   specified by `path` changes, this will trigger Meson to reconfigure the
   project. If the file specified by `path` is a `files()` object it
   cannot refer to a built file.

### relative_to

*Since 1.3.0*

Return a relative filepath. In the event a relative path could not be found, the
absolute path of `to` is returned. Relative path arguments will be assumed to be
relative to `meson.current_source_dir()`.

Has the following positional arguments:
   - to `str | file | custom_tgt | custom_idx | build_tgt`: end path
   - from `str | file | custom_tgt | custom_idx | build_tgt`: start path

returns:
   - a string

### copyfile

*Since 0.64.0*

Copy a file from the source directory to the build directory at build time

Has the following positional arguments:
   - src `File | str`: the file to copy

Has the following optional arguments:
   - dest `str`: the name of the output file. If unset will be the basename of
     the src argument

Has the following keyword arguments:
   - install `bool`: Whether to install the copied file, defaults to false
   - install_dir `str`: Where to install the file to
   - install_tag: `str`: the install tag to assign to this target
   - install_mode `array[str | int]`: the mode to install the file with

returns:
   - a [[custom_target]] object

```meson
copy = fs.copyfile('input-file', 'output-file')
```
