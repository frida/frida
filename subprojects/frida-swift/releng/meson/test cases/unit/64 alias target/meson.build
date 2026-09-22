project('alias target', 'c')

python3 = find_program('python3')

exe_target = executable('prog', 'main.c',
  build_by_default : false)

custom_target = custom_target('custom-target',
  output : 'hello.txt',
  command : [python3, '-c', 'print("hello")'],
  capture : true,
  build_by_default : false
)

alias_target('build-all', [exe_target, custom_target])

subdir('subdir')
