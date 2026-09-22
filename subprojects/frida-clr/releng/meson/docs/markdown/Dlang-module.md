# Dlang module

This module provides tools related to the D programming language.

## Usage

To use this module, just do: **`dlang = import('dlang')`**.
You can, of course, replace the name `dlang` with anything else.

The module only exposes one function, `generate_dub_file`, used to
automatically generate Dub configuration files.

### generate_dub_file()
This method only has two required arguments, the project name and the
source folder. You can pass other arguments with additional keywords,
they will be automatically translated to json and added to the
`dub.json` file.

**Structure**
```meson
generate_dub_file("project name", "source/folder", key: "value" ...)
```

**Example**
```meson
dlang = import('dlang')
dlang.generate_dub_file(meson.project_name().to_lower(), meson.source_root(),
                        authors: 'Meson Team',
                        description: 'Test executable',
                        copyright: 'Copyright © 2018, Meson Team',
                        license: 'MIT',
                        sourceFiles: 'test.d',
                        targetType: 'executable',
                        dependencies: my_dep
)
```

You can manually edit a Meson generated `dub.json` file or provide a
initial one. The module will only update the values specified in
`generate_dub_file()`.

Although not required, you will need to have a `description` and
`license` if you want to publish the package in the [D package
registry](https://code.dlang.org/).
