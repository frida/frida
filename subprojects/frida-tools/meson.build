project('frida-tools', 'c',
  version: run_command(find_program('python3'), files('setup.py'), '-V',
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.1.0',
)

python = import('python').find_installation()

node = find_program('node', version: '>=18.0.0', native: true, required: false)
if not node.found()
  error('Need Node.js >= 18.0.0 to process JavaScript code at build time')
endif
npm = find_program('npm', native: true, required: false)
if not npm.found()
  error('Need npm to process JavaScript code at build time')
endif

subdir('agents')
subdir('bridges')
subdir('apps')
subdir('frida_tools')
subdir('scripts')
subdir('completions')

pathsep = host_machine.system() == 'windows' ? ';' : ':'

test('frida-tools', python,
  args: ['-m', 'unittest', 'discover'],
  workdir: meson.current_source_dir(),
  env: {
    'PYTHONPATH': pathsep.join([
      meson.current_source_dir() / 'subprojects' / 'frida-python',
      meson.current_build_dir() / 'subprojects' / 'frida-python' / 'src',
    ]),
  },
  timeout: 30,
)
