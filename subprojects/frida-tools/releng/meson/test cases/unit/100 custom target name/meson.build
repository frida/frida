project('custom target name', 'c')

python = find_program('python3')

# Name argument is optional and should default to 'file.txt'
custom_target(
  input: 'file.txt.in',
  output: '@BASENAME@',
  command: [python, '--version'],
  build_by_default: true,
  capture: true,
)

subdir('subdir')
