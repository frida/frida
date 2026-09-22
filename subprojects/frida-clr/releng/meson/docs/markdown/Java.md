---
title: Java
short-description: Compiling Java programs
...

# Compiling Java applications

Meson has experimental support for compiling Java programs. The basic
syntax consists of only one function and would be used like this:

```meson
project('javaprog', 'java')

myjar = jar('mything', 'com/example/Prog.java',
            main_class : 'com.example.Prog')

test('javatest', myjar)
```

However note that Meson places limitations on how you lay out your code.

* all Java files for a jar must be under the subdirectory the jar definition is in
* all Java files must be in paths specified by their package, e.g. a class called `com.example.Something` must be in a Java file situated at `com/example/Something.java`.
* Meson only deals with jar files, you cannot poke individual class files (unless you do so manually)
