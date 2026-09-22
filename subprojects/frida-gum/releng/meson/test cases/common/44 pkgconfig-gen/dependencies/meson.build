project('pkgconfig-gen-dependencies', 'c', version: '1.0')

if find_program('pkg-config').version() == '2.0.1'
    error('MESON_SKIP_TEST: cannot test uninstalled.pc due to https://github.com/pkgconf/pkgconf/issues/310#issuecomment-1677844842')
endif

pkgg = import('pkgconfig')

# libmain internally use libinternal and expose libexpose in its API
exposed_lib = shared_library('libexposed', 'exposed.c')
internal_lib = shared_library('libinternal', 'internal.c')
main_lib = both_libraries('libmain', 'dummy.c', link_with : [exposed_lib, internal_lib])
custom_lib = shared_library('custom', 'custom.c')

pkgg.generate(exposed_lib)

# Declare a few different Dependency objects
pc_dep = dependency('libfoo', version : '>=1.0')
pc_dep_dup = dependency('libfoo', version : '>= 1.0')
notfound_dep = dependency('notfound', required : false)
threads_dep = dependency('threads')
custom_dep = declare_dependency(link_with : custom_lib, compile_args : ['-DCUSTOM'])
custom2_dep = declare_dependency(link_args : ['-lcustom2'], compile_args : ['-DCUSTOM2'])

exe = executable('test1', 'main.c', dependencies : [pc_dep])
test('Test1', exe)

# Generate a PC file:
# - Having libmain in libraries should pull implicitly libexposed and libinternal in Libs.private
# - Having libexposed in libraries should remove it from Libs.private
# - We generated a pc file for libexposed so it should be in Requires instead of Libs
# - Having threads_dep in libraries should add '-pthread' in both Libs and Cflags
# - Having custom_dep in libraries and libraries_private should only add it in Libs
# - Having custom2_dep in libraries_private should not add its Cflags
# - Having pc_dep in libraries_private should add it in Requires.private
# - pc_dep_dup is the same library and same version, should be ignored
# - notfound_dep is not required so it shouldn't appear in the pc file.
pkgg.generate(libraries : [main_lib, exposed_lib, threads_dep, threads_dep, custom_dep, custom_dep, '-pthread'],
  libraries_private : [custom_dep, custom2_dep, custom2_dep, pc_dep, pc_dep_dup, notfound_dep],
  version : '1.0',
  name : 'dependency-test',
  filebase : 'dependency-test',
  description : 'A dependency test.'
)

pkgg.generate(
  name : 'requires-test',
  version : '1.0',
  description : 'Dependency Requires field test.',
  requires : [exposed_lib, pc_dep, 'libhello'],
)

pkgg.generate(
  name : 'requires-private-test',
  version : '1.0',
  description : 'Dependency Requires.private field test.',
  requires_private : [exposed_lib, pc_dep, 'libhello', notfound_dep],
)

# Verify that if we promote internal_lib as public dependency, it comes after
# the main library.
main_lib2 = both_libraries('libmain2', 'dummy.c', link_with : internal_lib)
pkgg.generate(main_lib2,
  libraries : internal_lib,
  filebase : 'pub-lib-order',
)

# This will be built against both simple7.pc and simple7-uninstalled.pc, check
# that include directories works in both cases.
simple7 = dependency('simple7')
exe = executable('test2', 'test2.c',
  dependencies: simple7,
)
test('Test2', exe)
