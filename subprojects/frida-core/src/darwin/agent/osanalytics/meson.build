osanalytics_js = custom_target('osanalytics.js',
  input: [
    'package.json',
    'package-lock.json',
    'index.js',
  ],
  output: [
    'osanalytics.js',
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
