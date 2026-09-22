# Java Module

*(added in 0.60.0)*

## Functions

### `generate_native_header()`

*(deprecated in 0.62.0, use `generate_native_headers()`)*
*(removed in 1.0.0)*

This function will generate a header file for use in Java native module
development by reading the supplied Java file for `native` method declarations.

Keyword arguments:

- `package`: The [package](https://en.wikipedia.org/wiki/Java_package) of the
file. If left empty, Meson will assume that there is no package.

### `generate_native_headers()`

*(added in 0.62.0)*
*(deprecated in 1.0.0, use `native_headers()`)*

This function will generate native header files for use in Java native module
development by reading the supplied Java files for `native` method declarations.

Keyword arguments:

- `classes`: The list of class names relative to the `package`, if it exists,
which contain `native` method declarations. Use `.` separated class names.

- `package`: The [package](https://en.wikipedia.org/wiki/Java_package) of the
file. If left empty, Meson will assume that there is no package.

Example:

```java
// Outer.java

package com.mesonbuild;

public class Outer {
    private static native void outer();

    public static class Inner {
        private static native void inner();
    }
}
```

With the above file, an invocation would look like the following:

```meson
java = import('java')

native_headers = java.generate_native_headers(
    'Outer.java',
    package: 'com.mesonbuild',
    classes: ['Outer', 'Outer.Inner']
)
```

### `native_headers()`

*(added in 1.0.0)*

This function will generate native header files for use in Java native module
development by reading the supplied Java files for `native` method declarations.

Keyword arguments:

- `classes`: The list of class names relative to the `package`, if it exists,
which contain `native` method declarations. Use `.` separated class names.

- `package`: The [package](https://en.wikipedia.org/wiki/Java_package) of the
file. If left empty, Meson will assume that there is no package.

Example:

```java
// Outer.java

package com.mesonbuild;

public class Outer {
    private static native void outer();

    public static class Inner {
        private static native void inner();
    }
}
```

With the above file, an invocation would look like the following:

```meson
java = import('java')

native_headers = java.generate_native_headers(
    'Outer.java',
    package: 'com.mesonbuild',
    classes: ['Outer', 'Outer.Inner']
)
```
