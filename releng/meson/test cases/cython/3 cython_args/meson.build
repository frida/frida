project('cython_args', ['cython', 'c'],
  # Needed because Windows Python builds are release-only and tend to be
  # unhappy with a debug build type.
  default_options : ['buildtype=release']
)

if meson.backend() != 'ninja'
  error('MESON_SKIP_TEST: Ninja backend required')
endif

python = import('python').find_installation('python3')
python_dep = python.dependency()
if not python_dep.found()
  error('MESON_SKIP_TEST: Python library not found.')
endif

mod = python.extension_module(
  'cythonargs',
  files('cythonargs.pyx'),
  cython_args: [
    '--compile-time-env',
    'VALUE=1',
    '-3',
  ],
)

test(
  'test',
  python,
  args: [
    'test.py'
  ],
  workdir: meson.current_source_dir(),
  env: environment({
    'PYTHONPATH': meson.current_build_dir(),
  })
)
