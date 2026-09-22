---
short-description: Simple project step by step explanation
...

# Meson sample

A Meson file that builds an executable looks like this.

```meson
project('simple', 'c')
executable('myexe', 'source.c')
```

All Meson build definitions begin with the `project` command. It
specifies the name of the project and what programming languages it
uses. Here the project is called *simple* and it uses only the C
programming language. All strings are single-quoted.

On the next line we define a *build target*, in this case an
executable called *myexe*. It consists of one source file. This is all
the code that a user needs to write to compile an executable with
Meson.

Variables are fully supported. The above code snippet could also have
been declared like this.

```meson
project('simple', 'c')
src = 'source.c'
executable('myexe', src)
```

Most executables consist of more than one source file. The easiest way
to deal with this is to put them in an array.

```meson
project('simple', 'c')
src = ['source1.c', 'source2.c', 'source3.c']
executable('myexe', src)
```

Meson also supports the notion of *keyword arguments*. Indeed most
arguments to functions can only be passed using them. The above
snippet could be rewritten like this.

```meson
project('simple', 'c')
src = ['source1.c', 'source2.c', 'source3.c']
executable('myexe', sources : src)
```

These two formats are equivalent and choosing one over the other is
mostly a question of personal preference.

The `executable` command actually returns an *executable object*,
which represents the given build target. It can be passed on to other
functions, like this.

```meson
project('simple', 'c')
src = ['source1.c', 'source2.c', 'source3.c']
exe = executable('myexe', src)
test('simple test', exe)
```

Here we create a unit test called *simple test*, and which uses the
built executable. When the tests are run with the `meson test`
command, the built executable is run. If it returns zero, the test
passes. A non-zero return value indicates an error, which Meson will
then report to the user.

A note to Visual Studio users
-----

There's a slight terminology difference between Meson and Visual
Studio. A Meson *project* is the equivalent to a Visual Studio
*solution*. That is, the topmost thing that encompasses all things to
be built. A Visual Studio *project* on the other hand is the
equivalent of a Meson top level build target, such as an executable or
a shared library.
