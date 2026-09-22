project('has function', 'c', 'cpp')

host_system = host_machine.system()

# This is used in the `test_compiler_check_flags_order` unit test
unit_test_args = '-I/tmp'
defines_has_builtin = '''#ifndef __has_builtin
#error "no __has_builtin"
#endif
'''
compilers = [meson.get_compiler('c'), meson.get_compiler('cpp')]

foreach cc : compilers
  if not cc.has_function('printf', prefix : '#include<stdio.h>',
                         args : unit_test_args)
    error('"printf" function not found (should always exist).')
  endif

  # Should also be able to detect it without specifying the header
  # We check for a different function here to make sure the result is
  # not taken from a cache (ie. the check above)
  # On MSVC fprintf is defined as an inline function in the header, so it cannot
  # be found without the include.
  if not ['msvc', 'intel-cl'].contains(cc.get_id())
    assert(cc.has_function('fprintf', args : unit_test_args),
           '"fprintf" function not found without include (on !msvc).')
  else
    assert(cc.has_function('fprintf', prefix : '#include <stdio.h>',
                           args : unit_test_args),
           '"fprintf" function not found with include (on msvc).')
    # Compiler intrinsics
    assert(cc.has_function('strcmp'),
           'strcmp intrinsic should have been found on MSVC')
    assert(cc.has_function('strcmp', prefix : '#include <string.h>'),
           'strcmp intrinsic should have been found with #include on MSVC')
  endif

  if cc.has_function('hfkerhisadf', prefix : '#include<stdio.h>',
                     args : unit_test_args)
    error('Found nonexistent function "hfkerhisadf".')
  endif

  if cc.has_function('hfkerhisadf', args : unit_test_args)
    error('Found nonexistent function "hfkerhisadf".')
  endif

  # With glibc (before 2.32, see below) on Linux, lchmod is a stub that will
  # always return an error, we want to detect that and declare that the
  # function is not available.
  # We can't check for the C library used here of course, but the main
  # alternative Linux C library (musl) doesn't use glibc's stub mechanism;
  # also, it has implemented lchmod since 2013, so it should be safe to check
  # that lchmod is available on Linux when not using glibc.
  if host_system == 'linux' or host_system == 'darwin'
    assert (cc.has_function('poll', prefix : '#include <poll.h>',
                            args : unit_test_args),
            'couldn\'t detect "poll" when defined by a header')
    lchmod_prefix = '#include <sys/stat.h>\n#include <unistd.h>'
    has_lchmod = cc.has_function('lchmod', prefix : lchmod_prefix, args : unit_test_args)

    if host_system == 'linux'
      # __GLIBC__ macro can be retrieved by including almost any C library header
      glibc_major = cc.get_define('__GLIBC__', prefix: '#include <unistd.h>', args: unit_test_args)
      # __GLIBC__ will only be set for glibc
      if glibc_major != ''
        glibc_minor = cc.get_define('__GLIBC_MINOR__', prefix: '#include <unistd.h>', args: unit_test_args)
        glibc_vers = '@0@.@1@'.format(glibc_major, glibc_minor)
        message('GLIBC version:', glibc_vers)

        # lchmod was implemented in glibc 2.32 (https://sourceware.org/pipermail/libc-announce/2020/000029.html)
        if glibc_vers.version_compare('<2.32')
          assert (not has_lchmod, '"lchmod" check should have failed')
        else
          assert (has_lchmod, '"lchmod" check should have succeeded')
        endif
      else
        # Other C libraries for Linux should have lchmod
        assert (has_lchmod, '"lchmod" check should have succeeded')
      endif
    else
      # macOS and *BSD have lchmod
      assert (has_lchmod, '"lchmod" check should have succeeded')
    endif
    # Check that built-ins are found properly both with and without headers
    assert(cc.has_function('alloca', args : unit_test_args),
           'built-in alloca must be found on ' + host_system)
    assert(cc.has_function('alloca', prefix : '#include <alloca.h>',
           args : unit_test_args),
           'built-in alloca must be found with #include')
    if not cc.compiles(defines_has_builtin, args : unit_test_args)
      assert(not cc.has_function('alloca',
             prefix : '#include <alloca.h>\n#undef alloca',
             args : unit_test_args),
             'built-in alloca must not be found with #include and #undef')
    endif
  endif

  # For some functions one needs to define _GNU_SOURCE before including the
  # right headers to get them picked up. Make sure we can detect these functions
  # as well without any prefix
  if cc.has_header_symbol('sys/socket.h', 'recvmmsg',
                          prefix : '#define _GNU_SOURCE',
                          args : unit_test_args)
    # We assume that if recvmmsg exists sendmmsg does too
    assert (cc.has_function('sendmmsg', args : unit_test_args),
            'Failed to detect function "sendmmsg" (should always exist).')
  endif

  # We should be able to find GCC and Clang __builtin functions
  if ['gcc', 'clang'].contains(cc.get_id())
    # __builtin_constant_p is documented to exist at least as far back as
    # GCC 2.95.3
    assert(cc.has_function('__builtin_constant_p', args : unit_test_args),
           '__builtin_constant_p must be found under gcc and clang')
  endif
endforeach
