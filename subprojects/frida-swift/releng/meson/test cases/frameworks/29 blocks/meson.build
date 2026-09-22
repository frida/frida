project('blocks-dependency', 'c')

id = meson.get_compiler('c').get_id()
if id != 'clang' or build_machine.system() == 'windows'
  error('MESON_SKIP_TEST: Only clang on unix-like systems supports the blocks extension.')
endif

exe = executable('main', 'main.c',
  dependencies: dependency('blocks')
)

test('test-blocks', exe)
