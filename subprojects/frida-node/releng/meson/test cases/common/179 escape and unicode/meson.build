project('180 escape', 'c')

gen = generator(find_program('file.py'), arguments:['@INPUT@', 'erd\u0151', '@OUTPUT@'], output: '@BASENAME@')

gen_file = gen.process('file.c.in')

find_file_list = run_command(find_program('find.py'), check: true)
assert(find_file_list.returncode() == 0, 'Didn\'t find any files.')

# Strings should support both octal \ooo and hex \xhh encodings

found_files_oct = []
foreach l : find_file_list.stdout().strip('\0').split('\000')
  found_files_oct += [files(l)]
endforeach

test('first', executable('first', found_files_oct + [gen_file]))

found_files_hex = []
foreach l : find_file_list.stdout().strip('\x00').split('\x00')
  found_files_hex += [files(l)]
endforeach

test('second', executable('second', found_files_hex + [gen_file]))

# Unrecognized and malformed escape sequences are literal

malformed = [
 [ '\c', 'c' ],
 [ '\Uabcdefghi', 'Uabcdefghi'],
 [ '\u123 ', 'u123 '],
 [ '\xqr', 'xqr'],
]

foreach m : malformed
  assert(m[0].endswith(m[1]), 'bad escape sequence had unexpected end')
  assert(m[0].startswith('\\'), 'bad escape sequence had unexpected start')
endforeach
