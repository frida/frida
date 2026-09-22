---
short-description: Instructions on handling include directories
...

# Include directories

Most `C`/`C++` projects have headers in different directories than
sources. Thus you need to specify include directories. Let's assume
that we are at some subdirectory and wish to add its `include`
subdirectory to some target's search path. To create a include
directory object we do this:

```meson
incdir = include_directories('include')
```

The `incdir` variable now holds a reference to the `include` subdir.
Now we pass that as an argument to a build target:

```meson
executable('someprog', 'someprog.c', include_directories : incdir)
```

Note that these two commands can be given in any subdirectories and it
will still work. Meson will keep track of the locations and generate
proper compiler flags to make it all work.

Another thing to note is that `include_directories` adds both the
source directory and corresponding build directory to include path, so
you don't have to care.
