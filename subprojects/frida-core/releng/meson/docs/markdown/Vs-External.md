# Visual Studio's external build projects

Visual Studio supports developing projects that have an external build
system. If you wish to use this integration method, here is how you
set it up. This documentation describes Visual Studio 2019. Other
versions have not been tested, but they should work roughly in the
same way.

## Creating and compiling

Check out your entire project in some directory. Then open Visual
Studio and select `File -> New -> Project` and from the list of
project types select `Makefile project`. Click `Next`.

Type your project's name In the `Project name` entry box. In this
example we're going to use `testproj`. Next select the `Location`
entry and browse to the root of your project sources. Make sure that
the checkbox `Place solution and project in the same directory` is
checked. Click `Create`.

The next dialog page defines build commands, which you should set up
as follows:

| entry | value |
| ----- | ----- |
|build  | `meson compile -C $(Configuration)` |
|clean  | `meson compile -C $(Configuration) --clean` |
|rebuild| `meson compile -C $(Configuration) --clean && meson compile -C $(Configuration)` |
|Output | `$(Configuration)\name_of_your_executable.exe` |


Then click `Finish`.

Visual Studio has created a subdirectory in your source root. It is
named after the project name. In this case it would be `testproj`. Now
you need to set up Meson for building both Debug and Release versions
in this directory. Open a VS dev tool terminal, go to the source root
and issue the following commands.

```
meson testproj\Debug
meson testproj\Release --buildtype=debugoptimized
```

Now you should have a working VS solution that compiles and runs both
in Debug and Release modes.

## Adding sources to the project

This project is not very useful on its own, because it does not list
any source files. VS does not seem to support adding entire source
trees at once, so you have to add sources to the solution manually.

In the main view go to `Solution Explorer`, right click on the project
you just created and select `Add -> Existing Item`, browse to your
source tree and select all files you want to have in this project. Now
you can use the editor and debugger as in a normal VS project.
