project('test', 'c')

python = find_program('python3')

lib = library('foo', 'foo.c')

# The library does not yet exist but we can already use its path during
# configuration. This should not trigger a reconfigure when the library is
# rebuilt.
configure_file(
  output: 'out.txt',
  capture: true,
  command: [python, '-c', 'import sys; print(sys.argv[1])', lib.full_path()],
)

message('Project configured')
