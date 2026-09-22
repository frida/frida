# Unstable Wayland Module

This module is available since version 0.62.0.

This module provides helper functions to find wayland protocol
xmls and to generate .c and .h files using wayland-scanner

**Note**: this module is unstable. It is only provided as a technology
preview. Its API may change in arbitrary ways between releases or it
might be removed from Meson altogether.

## Quick Usage

```meson
project('hello-wayland', 'c')

wl_dep = dependency('wayland-client')
wl_mod = import('unstable-wayland')

xml = wl_mod.find_protocol('xdg-shell')
xdg_shell = wl_mod.scan_xml(xml)

executable('hw', 'main.c', xdg_shell, dependencies : wl_dep)
```

## Methods

### find_protocol
```meson
xml = wl_mod.find_protocol(
  'xdg-decoration',
  state : 'unstable',
  version : 1,
)
```
This function requires one positional argument: the protocol base name.

It takes the following keyword arguments:
- `state` Optional arg that specifies the current state of the protocol.
  Either `'stable'`, `'staging'`, or `'unstable'`. The default is `'stable'`.
- `version` The backwards incompatible version number as integer.
  Required for staging and unstable, but not allowed for stable.

**Returns**: a [[@file]] that can be passed to [scan_xml](#scan_xml)

### scan_xml
```meson
generated = wl_mod.scan_xml(
  'my-protocol.xml',
  client : true,
  server : true,
  public : false,
  include_core_only : true,
)
```
This function accepts one or more arguments of either string or file type.

It takes the following keyword arguments:
- `public` Optional arg that specifies the scope of the generated code.
  The default is false.
- `client` Optional arg that specifies if client side header file is
  generated. The default is true.
- `server` Optional arg that specifies if server side header file is
  generated. The default is false.
- `include_core_only` Optional arg that specifies that generated headers only include
  `wayland-<client|server>-core.h` instead of `wayland-<client|server>.h`. 
  The default is true. Since *0.64.0*

**Returns**: a list of [[@custom_tgt]] in the order source, client side header,
server side header. Generated header files have the name
`<name>-<client|server>-protocol.h`.

## Links
- [Official Wayland Documentation](https://wayland.freedesktop.org/docs/html/)
- [Wayland GitLab](https://gitlab.freedesktop.org/wayland)
- [Wayland Book](https://wayland-book.com/)
