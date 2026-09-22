## Wayland stable protocols can be versioned

The wayland module now accepts a version number for stable protocols.

```meson
wl_mod = import('unstable-wayland')

wl_mod.find_protocol(
  'linux-dmabuf',
  state: 'stable'
  version: 1
)
```
