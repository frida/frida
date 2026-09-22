---
short-description: Configuring a pre-generated build directory
...

# Configuring a build directory

Often you want to change the settings of your build after it has been
generated. For example you might want to change from a debug build
into a release build, set custom compiler flags, change the build
options provided in your `meson.options` file and so on.

The main tool for this is the `meson configure` command.

You invoke `meson configure` by giving it the location of your build
dir. If omitted, the current working directory is used instead. Here's
a sample output for a simple project.

    Core properties

    Source dir /home/jpakkane/clangdemo/2_address
    Build dir  /home/jpakkane/clangdemo/2_address/buildmeson

    Core options:
      Option          Current Value Possible Values                                            Description
      ------          ------------- ---------------                                            -----------
      auto_features   auto          [enabled, disabled, auto]                                  Override value of all 'auto' features
      backend         ninja         [ninja, vs, vs2010, vs2015, vs2017, vs2019, vs2022, xcode] Backend to use
      buildtype       release       [plain, debug, debugoptimized, release, minsize, custom]   Build type to use
      debug           false         [true, false]                                              Debug
      default_library shared        [shared, static, both]                                     Default library type
      install_umask   0022          [preserve, 0000-0777]                                      Default umask to apply on permissions of installed files
      layout          mirror        [mirror, flat]                                             Build directory layout
      optimization    3             [plain, 0, g, 1, 2, 3, s]                                  Optimization level
      prefer_static   false         [true, false]                                              Whether to try static linking before shared linking
      strip           false         [true, false]                                              Strip targets on install
      unity           off           [on, off, subprojects]                                     Unity build
      warning_level   1             [0, 1, 2, 3, everything]                                   Compiler warning level to use
      werror          false         [true, false]                                              Treat warnings as errors

    Backend options:
      Option            Current Value Possible Values Description
      ------            ------------- --------------- -----------
      backend_max_links 0             >=0             Maximum number of linker processes to run or 0 for no limit

    Base options:
      Option      Current Value Possible Values                                               Description
      ------      ------------- ---------------                                               -----------
      b_asneeded  true          [true, false]                                                 Use -Wl,--as-needed when linking
      b_colorout  always        [auto, always, never]                                         Use colored output
      b_coverage  false         [true, false]                                                 Enable coverage tracking.
      b_lto       false         [true, false]                                                 Use link time optimization
      b_lundef    true          [true, false]                                                 Use -Wl,--no-undefined when linking
      b_ndebug    false         [true, false, if-release]                                     Disable asserts
      b_pch       true          [true, false]                                                 Use precompiled headers
      b_pgo       off           [off, generate, use]                                          Use profile guided optimization
      b_sanitize  none          [none, address, thread, undefined, leak, memory, address,undefined] Code sanitizer to use
      b_staticpic true          [true, false]                                                 Build static libraries as position independent

    Compiler options:
      Option        Current Value Possible Values                                                                                               Description
      ------        ------------- ---------------                                                                                               -----------
      c_args        []                                                                                                                          Extra arguments passed to the C compiler
      c_link_args   []                                                                                                                          Extra arguments passed to the C linker
      c_std         c99           [none, c89, c99, c11, c17, c18, c2x, c23, gnu89, gnu99, gnu11, gnu17, gnu18, gnu2x, gnu23]                                C language standard to use
      cpp_args      []                                                                                                                          Extra arguments passed to the C++ compiler
      cpp_debugstl  false         [true, false]                                                                                                 STL debug mode
      cpp_link_args []                                                                                                                          Extra arguments passed to the C++ linker
      cpp_std       c++11         [none, c++98, c++03, c++11, c++14, c++17, c++1z, c++2a, c++20, gnu++03, gnu++11, gnu++14, gnu++17, gnu++1z, gnu++2a, gnu++20] C++ language standard to use
      fortran_std   []            [none, legacy, f95, f2003, f2008, f2018]                                                                      language standard to use

    Directories:
      Option         Current Value        Description
      ------         -------------        -----------
      bindir         bin                  Executable directory
      datadir        share                Data file directory
      includedir     include              Header file directory
      infodir        share/info           Info page directory
      libdir         lib/x86_64-linux-gnu Library directory
      libexecdir     libexec              Library executable directory
      localedir      share/locale         Locale data directory
      localstatedir  /var/local           Localstate data directory
      mandir         share/man            Manual page directory
      prefix         /usr/local           Installation prefix
      sbindir        sbin                 System executable directory
      sharedstatedir /var/local/lib       Architecture-independent data directory
      sysconfdir     etc                  Sysconf data directory

    Project options:
      Option         Current Value Possible Values           Description
      ------         ------------- ---------------           -----------
      array_opt      [one, two]    [one, two, three]         array_opt
      combo_opt      three         [one, two, three]         combo_opt
      free_array_opt [one, two]                              free_array_opt
      integer_opt    3             >=0, <=5                  integer_opt
      other_one      false         [true, false]             other_one
      some_feature   enabled       [enabled, disabled, auto] some_feature
      someoption     optval                                  An option

    Testing options:
      Option    Current Value Possible Values Description
      ------    ------------- --------------- -----------
      errorlogs true          [true, false]   Whether to print the logs from failing tests
      stdsplit  true          [true, false]   Split stdout and stderr in test logs

These are all the options available for the current project arranged
into related groups. The first column in every field is the name of
the option. To set an option you use the `-D` option. For example,
changing the installation prefix from `/usr/local` to `/tmp/testroot`
you would issue the following command.

    meson configure -Dprefix=/tmp/testroot

Then you would run your build command (usually `meson compile`), which
would cause Meson to detect that the build setup has changed and do
all the work required to bring your build tree up to date.

Since 0.50.0, it is also possible to get a list of all build options
by invoking [`meson configure`](Commands.md#configure) with the
project source directory or the path to the root `meson.build`. In
this case, Meson will print the default values of all options similar
to the example output from above.
