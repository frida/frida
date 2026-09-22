# Name argument is optional and should default to 'file.txt', but should be
# displayed as 'subdir/file.txt' by ninja.
custom_target(
  input: '../file.txt.in',
  output: ['@BASENAME@'],
  command: [python, '--version'],
  build_by_default: true,
  capture: true,
)
