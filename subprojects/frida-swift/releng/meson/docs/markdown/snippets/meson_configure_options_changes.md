## Meson configure handles changes to options in more cases

Meson configure now correctly handles updates to the options file without a full
reconfigure. This allows making a change to the `meson.options` or
`meson_options.txt` file without a reconfigure.

For example, this now works:
```sh
meson setup builddir
git pull
meson configure builddir -Doption-added-by-pull=value
```
