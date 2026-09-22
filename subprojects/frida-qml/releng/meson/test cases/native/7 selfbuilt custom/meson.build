project('selfbuilt custom', 'cpp')

# Build an exe and use it in a custom target
# whose output is used to build a different exe.

tool = executable('tool', 'tool.cpp', native : true)

hfile = custom_target('datah',
  output : 'data.h',
  input : 'data.dat',
  command : [tool, '@INPUT@', '@OUTPUT@'],
)

main = executable('mainprog', 'mainprog.cpp', hfile)

test('maintest', main)

lib = library('libtool', 'tool.cpp')

checkarg = executable('checkarg', 'checkarg.cpp', native : true)

ctlib = custom_target('ctlib',
  output : 'ctlib.out',
  capture : true,
  command : [checkarg, lib],
  build_by_default : true,
)

if meson.is_cross_build() and meson.can_run_host_binaries()
  checkarg_host = executable('checkarg_host', 'checkarg.cpp')

  ctlib_host = custom_target(
    'ctlib_host',
    output : 'ctlib.host.out',
    capture : true,
    command : [checkarg_host, lib],
    build_by_default : true,
  )
endif
