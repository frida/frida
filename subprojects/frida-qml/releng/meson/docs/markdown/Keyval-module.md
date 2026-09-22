---
short-description: Keyval module
authors:
    - name: Mark Schulte, Paolo Bonzini
      years: [2017, 2019]
      has-copyright: false
...

# keyval module

This module parses files consisting of a series of `key=value` lines.
One use of this module is to load kconfig configurations in Meson
projects.

**Note**: this does not provide kconfig frontend tooling to generate a
configuration. You still need something such as kconfig frontends (see
link below) to parse your Kconfig files, and then (after you've chosen
the configuration options), output a ".config" file.

  [kconfig-frontends]: http://ymorin.is-a-geek.org/projects/kconfig-frontends

## Usage

The module may be imported as follows:

``` meson
keyval = import('keyval')
```

The following functions will then be available as methods on the object
with the name `keyval`. You can, of course, replace the name
`keyval` with anything else.

### keyval.load()

This function loads a file consisting of a series of `key=value` lines
and returns a dictionary object.

`keyval.load()` makes no attempt at parsing the values in the file. In
particular boolean and integer values will be represented as strings,
and strings will keep any quoting that is present in the input file.
It can be useful to create a
[`configuration_data()`](#configuration_data) object from the
dictionary and use methods such as `get_unquoted()`.

Kconfig frontends usually have ".config" as the default name for the
configuration file. However, placing the configuration file in the
source directory limits the user to one configuration per source
directory. In order to allow separate configurations for each build
directory, as is the Meson standard, `meson.build` should not hardcode
".config" as the argument to `kconfig.load()`, and should instead make
the argument to `kconfig.load()` a [project build
option](Build-options.md).

* The first (and only) argument is the path to the configuration file to
  load (usually ".config").

**Returns**: a [[@dict]] object.
