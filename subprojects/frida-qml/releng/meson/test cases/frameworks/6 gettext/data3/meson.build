
if itstool.found()

  mi_translated = i18n.itstool_join(
    input:  'com.mesonbuild.test.intlprog.metainfo.xml',
    output: 'com.mesonbuild.test.intlprog.metainfo.xml',
    mo_targets: mo_targets,
    its_files: ['metainfo.its'],
    install: true,
    install_dir: get_option('datadir') / 'metainfo',
  )

  # older versions of itstool have a bug where ITS rules specified on the command-line
  # are not read when joining files. Since we don't install appstream in the Meson CI
  # environment, the to-be-tested entry will be untranslated and the test would fail, so
  # we just skip verification if the installed itstool is too old.
  r = run_command(itstool, '-v', check: true)
  itstool_v = r.stdout().strip().split()
  if itstool_v[1].version_compare('>=2.0.6')
    verify_exe = find_program('verify.py')
    test('test xml translation',
        verify_exe,
        args: [mi_translated,
              '<p xml:lang="de">Dies ist <code>Text</code> mit <em>eingebetteten XML Tags</em>. Toll!</p>']
    )
  else
    message('Skipping translation verification: Itstool too old.')
  endif

else
  install_data('com.mesonbuild.test.intlprog.metainfo.xml',
      install_dir: get_option('datadir') / 'metainfo')
endif
