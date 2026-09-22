entrypoint = 'agent.ts'

sources = [
  entrypoint,
  'value.ts',
  'package.json',
  'package-lock.json',
  'tsconfig.json',
]

custom_target('repl-agent.js',
  input: sources,
  output: ['repl_agent.js'],
  command: [
    build_agent,
    '@INPUT@',
    '@OUTPUT0@',
    '@PRIVATE_DIR@',
  ],
  install: true,
  install_dir: python.get_install_dir() / 'frida_tools',
)
