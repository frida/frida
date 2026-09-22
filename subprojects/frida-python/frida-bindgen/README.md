# frida-bindgen

Language-agnostic core for Frida's `.gir`-driven binding generators.

`frida_bindgen_core` holds the shared substrate consumed by frida-python and
frida-node: the introspection model parsed from Frida's GObject-Introspection
`.gir` files, the naming helpers, and the loader. Each binding vendors this
package as a git submodule and layers its own language-specific subclasses,
code generator, and customizations on top via the `Factory` seam.
