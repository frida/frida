project(
  'cross test passed',
  'c',
  version : '>= 0.51'
)

e = executable('exec', 'src/main.c')

py = find_program('python3')

test('root', e)
test('main', py, args : [meson.current_source_dir() / 'script.py', e])

wrapper_args = []
if get_option('expect')
  wrapper_args += '--expected'
endif

test('exe_wrapper in env', py, args : [meson.current_source_dir() / 'exewrapper.py', e, wrapper_args])
