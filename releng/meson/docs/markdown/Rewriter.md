---
short-description: Automatic modification of the build system files
...

# Meson file rewriter

Since version 0.50.0, Meson has the functionality to perform some
basic modification on the `meson.build` files from the command line.
The currently supported operations are:

- For build targets:
  - Add/Remove source files
  - Add/Remove targets
  - Modify a select set of kwargs
  - Print some JSON information
- For dependencies:
  - Modify a select set of kwargs
- For the project function:
  - Modify a select set of kwargs
  - Modify the default options list

The rewriter has both, a normal command line interface and a "script
mode". The normal CLI is mostly designed for everyday use. The "script
mode", on the other hand, is meant to be used by external programs
(IDEs, graphical frontends, etc.)

The rewriter itself is considered stable, however the user interface
and the "script mode" API might change in the future. These changes
may also break backwards compatibility to older releases.

We are also open to suggestions for API improvements.

## Using the rewriter

All rewriter functions are accessed via `meson rewrite`. The Meson
rewriter assumes that it is run inside the project root directory. If
this isn't the case, use `--sourcedir` to specify the actual project
source directory.

### Adding and removing sources

The most common operations will probably be the adding and removing of source
files to a build target. This can be easily done with:

```bash
meson rewrite target <target name/id> {add/rm} [list of sources]
```

For instance, given the following example

```meson
src = ['main.cpp', 'fileA.cpp']

exe1 = executable('testExe', src)
```

the source `fileB.cpp` can be added with:

```bash
meson rewrite target testExe add fileB.cpp
```

After executing this command, the new `meson.build` will look like this:

```meson
src = ['main.cpp', 'fileA.cpp', 'fileB.cpp']

exe1 = executable('testExe', src)
```

In this case, `exe1` could also have been used for the target name.
This is possible because the rewriter also searches for assignments
and unique Meson IDs, which can be acquired with introspection. If
there are multiple targets with the same name, Meson will do nothing
and print an error message.

For more information see the help output of the rewriter target
command.

### Adding and removing `extra_files`

*Since 0.61.0*

In the same way you can add and remove source files from a target, you can modify a target's
`extra_files` list:

```bash
meson rewrite target <target name/id> {add_extra_files/rm_extra_files} [list of extra files]
```

### Setting the project version

It is also possible to set kwargs of specific functions with the
rewriter. The general command for setting or removing kwargs is:

```bash
meson rewrite kwargs {set/delete} <function type> <function ID> <key1> <value1> <key2> <value2> ...
```

For instance, setting the project version can be achieved with this command:

```bash
meson rewrite kwargs set project / version 1.0.0
```

Currently, only the following function types are supported:

- dependency
- target (any build target, the function ID is the target name/ID)
- project (the function ID must be `/` since project() can only be called once)

For more information see the help output of the rewrite kwargs command.

Note msys bash may expand `/` to a path. Passing `//` will be
converted to `/` by msys bash but in order to keep usage
shell-agnostic, the rewrite command also allows `//` as the function
ID such that it will work in both msys bash and other shells.

### Setting the project default options

For setting and deleting default options, use the following command:

```bash
meson rewrite default-options {set/delete} <opt1> <value1> <opt2> <value2> ...
```

## Limitations

Rewriting a Meson file is not guaranteed to keep the indentation of
the modified functions. Additionally, comments inside a modified
statement will be removed. Furthermore, all source files will be
sorted alphabetically.

For instance adding `e.c` to srcs in the following code

```meson
# Important comment

srcs = [
'a.c', 'c.c', 'f.c',
# something important about b
       'b.c', 'd.c', 'g.c'
]

# COMMENT
```

would result in the following code:

```meson
# Important comment

srcs = [
  'a.c',
  'b.c',
  'c.c',
  'd.c',
  'e.c',
  'f.c',
  'g.c'
]

# COMMENT
```

## Using the "script mode"

The "script mode" should be the preferred API for third party
programs, since it offers more flexibility and higher API stability.
The "scripts" are stored in JSON format and executed with `meson
rewrite command <JSON file or string>`.

The JSON format is defined as follows:

```json
[
  {
    "type": "function to execute",
    ...
  }, {
    "type": "other function",
    ...
  },
  ...
]
```

Each object in the main array must have a `type` entry which specifies which
function should be executed.

Currently, the following functions are supported:

- target
- kwargs
- default_options

### Target modification format

The format for the type `target` is defined as follows:

```json
{
  "type": "target",
  "target": "target ID/name/assignment variable",
  "operation": "one of ['src_add', 'src_rm', 'target_rm', 'target_add', 'extra_files_add', 'extra_files_rm', 'info']",
  "sources": ["list", "of", "source", "files", "to", "add, remove"],
  "subdir": "subdir where the new target should be added (only has an effect for operation 'tgt_add')",
  "target_type": "function name of the new target -- same as in the CLI (only has an effect for operation 'tgt_add')"
}
```

The keys `sources`, `subdir` and `target_type` are optional.

### kwargs modification format

The format for the type `target` is defined as follows:

```json
{
  "type": "kwargs",
  "function": "one of ['dependency', 'target', 'project']",
  "id": "function ID",
  "operation": "one of ['set', 'delete', 'add', 'remove', 'remove_regex', 'info']",
  "kwargs": {
    "key1": "value1",
    "key2": "value2",
    ...
  }
}
```

### Default options modification format

The format for the type `default_options` is defined as follows:

```json
{
  "type": "default_options",
  "operation": "one of ['set', 'delete']",
  "options": {
    "opt1": "value1",
    "opt2": "value2",
    ...
  }
}
```

For operation `delete`, the values of the `options` can be anything
(including `null`)

## Extracting information

The rewriter also offers operation `info` for the types `target` and
`kwargs`. When this operation is used, Meson will print a JSON dump to
stderr, containing all available information to the rewriter about the
build target / function kwargs in question.

The output format is currently experimental and may change in the future.
