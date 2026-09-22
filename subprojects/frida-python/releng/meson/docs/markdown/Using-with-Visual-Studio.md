---
short-description: How to use Meson in Visual Studio
...

# Using with Visual Studio

In order to generate Visual Studio projects, Meson needs to know the
settings of your installed version of Visual Studio. The only way to
get this information is to run Meson under the Visual Studio Command
Prompt.

You can always find the Visual Studio Command Prompt by searching from
the Start Menu. However, the name is different for each Visual Studio
version. With Visual Studio 2019, look for "x64 Native Tools Command
Prompt for VS 2019". The next steps are [the same as
always](https://mesonbuild.com/Running-Meson.html#configuring-the-build-directory):

1. `cd` into your source directory
1. `meson setup builddir`, which will create and setup the build directory
1. `meson compile -C builddir`, to compile your code. You can also use `ninja -C builddir` here if you are using the default Ninja backend.

If you wish to generate Visual Studio project files, pass `--backend
vs`. At the time of writing the Ninja backend is more mature than the
VS backend so you might want to use it for serious work.

# Using Clang-CL with Visual Studio

*(new in 0.52.0)*

You will first need to get a copy of llvm+clang for Windows, such versions
are available from a number of sources, including the llvm website. Then you
will need the [llvm toolset extension for visual
studio](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.llvm-toolchain).
You then need to either use a [native file](Native-environments.md#binaries)
or `set CC=clang-cl`, and `set CXX=clang-cl` to use those compilers, Meson
will do the rest.

This only works with visual studio 2017 and 2019.

There is currently no support in Meson for clang/c2.

# Using Intel-CL (ICL) with Visual Studio

*(new in 0.52.0)*

To use ICL you need only have ICL installed and launch an ICL development
shell like you would for the ninja backend and Meson will take care of it.
