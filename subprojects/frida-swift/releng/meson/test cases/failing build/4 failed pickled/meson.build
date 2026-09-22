project('failed pickled command')

custom_target('failure',
  command: [find_program('false.py'), '\n'],
  output: 'output.txt',
  build_by_default: true,
)
