project('complex link cases', 'c')

cc = meson.get_compiler('c')

# In all tests, e1 uses s3 which uses s2 which uses s1.

# Executable links with s3 and s1 but not s2 because it is included in s3.
s1 = static_library('t1-s1', 's1.c')
s2 = static_library('t1-s2', 's2.c', link_with: s1)
s3 = static_library('t1-s3', 's3.c', link_whole: s2)
e = executable('t1-e1', 'main.c', link_with: s3)

# s3 is installed but not s1 so it has to include s1 too.
# Executable links only s3 because it contains s1 and s2.
s1 = static_library('t2-s1', 's1.c')
s2 = static_library('t2-s2', 's2.c', link_with: s1)
s3 = static_library('t2-s3', 's3.c', link_whole: s2, install: true)
e = executable('t2-e1', 'main.c', link_with: s3)

# Executable needs to link with s3 only
s1 = static_library('t3-s1', 's1.c')
s2 = static_library('t3-s2', 's2.c', link_with: s1)
s3 = shared_library('t3-s3', 's3.c', link_with: s2)
e = executable('t3-e1', 'main.c', link_with: s3)

# Executable needs to link with s3 and s2
s1 = static_library('t4-s1', 's1.c')
s2 = shared_library('t4-s2', 's2.c', link_with: s1)
s3 = static_library('t4-s3', 's3.c', link_with: s2)
e = executable('t4-e1', 'main.c', link_with: s3)

# Executable needs to link with s3 and s1
s1 = shared_library('t5-s1', 's1.c')
s2 = static_library('t5-s2', 's2.c', link_with: s1)
s3 = static_library('t5-s3', 's3.c', link_with: s2, install: true)
e = executable('t5-e1', 'main.c', link_with: s3)

# Executable needs to link with s3 and s2
s1 = static_library('t6-s1', 's1.c')
s2 = static_library('t6-s2', 's2.c', link_with: s1, install: true)
s3 = static_library('t6-s3', 's3.c', link_with: s2, install: true)
e = executable('t6-e1', 'main.c', link_with: s3)

# Regression test: s1 gets promoted to link_whole and that used to make all other
# libraries in the list (s2) to be ignored.
# Executable only needs to link with s3.
# See https://github.com/mesonbuild/meson/issues/11956.
s1 = static_library('t7-s1', 's1.c')
s2 = static_library('t7-s2', 's2.c')
s3 = static_library('t7-s3', 's3.c', link_with: [s1, s2], install: true)
e = executable('t7-e1', 'main.c', link_with: s3)

# Regression test: s3 should come last in the linker command. This seems to be
# required for at least backward compatibility reasons:
# https://github.com/mesonbuild/meson/pull/11957#issuecomment-1629243208
s1 = static_library('t8-s1', 's1.c')
s2 = static_library('t8-s2', 's2.c')
s3 = static_library('t8-s3', 's3.c')
e = executable('t8-e1', 'main.c',
  link_with: [s1, s2],
  dependencies: declare_dependency(link_with: s3),
)

if cc.get_argument_syntax() == 'gcc'
  # s1 is an internal static library, using custom target.
  s1_o = custom_target(
    input: 's1.c',
    output: 's1.c.o',
    command: [cc.cmd_array(), '-c', '-o', '@OUTPUT@', '@INPUT@']
  )
  s1 = custom_target(
      output: 'libt9-s1.a',
      command: ['ar', 'rcs', '@OUTPUT@', s1_o],
  )

  # Executable needs to link with s1, s2 and s3.
  s2 = static_library('t9-s2', 's2.c', link_with: s1)
  s3 = static_library('t9-s3', 's3.c', link_with: s2)
  e = executable('t9-e1', 'main.c', link_with: s3)

  # s2 cannot be installed because s1 is not being installed and Meson cannot
  # extract object files from the custom target.
  testcase expect_error('Cannot link_whole a custom or Rust target \'libt9-s1.a\' into a static library \'t10-s2\'. Instead, pass individual object files with the "objects:" keyword argument if possible. Meson had to promote link to link_whole because \'t10-s2\' is installed but not \'libt9-s1.a\', and thus has to include objects from \'libt9-s1.a\' to be usable.')
    s2 = static_library('t10-s2', 's2.c', link_with: s1, install: true)
  endtestcase

  # s3 cannot be installed because s1 is not being installed and Meson cannot
  # extract object files from the custom target.
  testcase expect_error('Cannot link_whole a custom or Rust target \'libt9-s1.a\' into a static library \'t11-s3\'. Instead, pass individual object files with the "objects:" keyword argument if possible. Meson had to promote link to link_whole because \'t11-s3\' is installed but not \'libt9-s1.a\', and thus has to include objects from \'libt9-s1.a\' to be usable.')
      s2 = static_library('t11-s2', 's2.c', link_with: s1)
      s3 = static_library('t11-s3', 's3.c', link_with: s2, install: true)
  endtestcase

  # s1 is an installed static library, using custom target.
  s1 = custom_target(
      output: 'libt12-s1.a',
      command: ['ar', 'rcs', '@OUTPUT@', s1_o],
      install: true,
      install_dir: get_option('libdir'),
  )

  # Executable needs to link with s1, s2 and s3.
  s2 = static_library('t12-s2', 's2.c', link_with: s1, install: true)
  s3 = static_library('t12-s3', 's3.c', link_with: s2)
  e = executable('t12-e1', 'main.c', link_with: s3)

  # Executable links with s3 and s1 but not s2 because it is included in s3.
  s2 = static_library('t13-s2', 's2.c', link_with: s1)
  s3 = static_library('t13-s3', 's3.c', link_with: s2, install: true)
  e = executable('t13-e1', 'main.c', link_with: s3)
endif
