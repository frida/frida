project('mixed', 'c', 'swift')
add_project_arguments('-embed-bitcode', language : 'swift')
subdir('mylib')
exe = executable('prog', 'main.swift',
  dependencies : dep)
test('c module', exe)
