reportcrash_js = custom_target('reportcrash.js',
  input: [
    'package.json',
    'package-lock.json',
    'index.js',
  ],
  output: [
    'reportcrash.js',
  ],
  command: [
    python,
    compile_js_agent,
    '@OUTPUT@',
    '@PRIVATE_DIR@',
    npm,
    '@INPUT@',
  ],
)
