---
short-description: Meson modules for common build operations
...

# Modules

In addition to core language features, Meson also provides a module
system aimed at providing helper methods for common build operations.
Using modules is simple, first you import them:

```meson
mymod = import('somemodule')
```

After this you can use the returned object to use the functionality
provided:

```meson
mymod.do_something('text argument')
```

Meson has a selection of modules to make common requirements easy to
use. Modules can be thought of like the standard library of a
programming language. Currently Meson provides the modules listed on
subpages.
