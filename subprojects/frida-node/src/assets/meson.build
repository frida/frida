adjusted_package_json = custom_target('package.json',
  input: package_json,
  output: 'package.json',
  command: [
    python,
    files('adjust-version.py'),
    version,
    '@INPUT@',
    '@OUTPUT@',
  ],
  install: true,
  install_dir: pkg_install_dir,
)
