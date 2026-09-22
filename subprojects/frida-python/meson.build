project('frida-python', 'c',
  version: run_command(find_program('python3'), files('setup.py'), '-V',
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.3.0',
)

python = import('python').find_installation()

cc = meson.get_compiler('c')

frida_component_cflags = []
ndebug = get_option('b_ndebug')
if ndebug == 'true' or (ndebug == 'if-release' and not get_option('debug'))
  frida_component_cflags += [
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CHECKS',
    '-DG_DISABLE_CAST_CHECKS',
  ]
endif

python_dep = python.dependency()
frida_core_dep = dependency('frida-core-1.0', default_options: [
  'frida_version=' + meson.project_version().replace('.dev', '-dev.'),
])

os_deps = []
host_os_family = host_machine.system()
if host_os_family != 'windows'
  os_deps += dependency('gio-unix-2.0')
endif

subdir('frida')

#test('frida-python', python,
#  args: ['-m', 'unittest', 'discover'],
#  workdir: meson.current_source_dir(),
#  env: {'PYTHONPATH': meson.current_build_dir() / 'src'},
#  timeout: 30,
#)
