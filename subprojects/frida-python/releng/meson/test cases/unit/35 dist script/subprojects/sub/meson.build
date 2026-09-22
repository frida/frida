project('sub', 'c',
  version : 'vcs')

if get_option('broken_dist_script')
  # Make sure we can add a dist script in a subproject, but it won't be run
  # if not using --include-subprojects.
  meson.add_dist_script('dist-script.py', 'broken')
else
  # The dist script replace prog.c with something that actually build.
  meson.add_dist_script('dist-script.py', 'success')
  executable('prog', 'prog.c')

  versiontest = find_program('version-test.py')
  test('dist version replacement', versiontest, args : meson.project_version())
endif
