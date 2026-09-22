project('preprocess', 'c')

cc = meson.get_compiler('c')

add_project_arguments(['-DFOO=0', '-DBAR=0'], language: 'c')

fs = import('fs')
bar_content = fs.read('bar.c')
bar_x = custom_target(
  input: 'bar.c',
  output: 'bar.x',
  command: ['python3', '-c', '''import sys;print(sys.argv[1].replace('@BAR@', 'bar'))''', bar_content],
  capture: true,
)

foo_h = import('fs').copyfile('foo.x', 'foo.h')

dep = declare_dependency(compile_args: '-DPLOP=0')

pp_files = cc.preprocess('foo.c', bar_x, 'math.c',
  output: '@PLAINNAME@.c',
  dependencies: dep,
  depends: foo_h,
  compile_args: ['-DBAZ=0'],
)

foreach f : pp_files
  message(f.full_path())
endforeach

subdir('src')

test('test-foo', executable('app', pp_files, link_depends: file_map))
