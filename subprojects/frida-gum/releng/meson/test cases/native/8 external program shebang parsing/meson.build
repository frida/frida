project('shebang parsing', 'c')

interpreter = executable('aninterp', 'main.c', native : true)

cdata = configuration_data()
cdata.set('INTRP', interpreter.full_path())

f = configure_file(input : 'script.int.in',
                   output : 'script.int',
                   configuration : cdata)

# Test that parsing a shebang with spaces works properly. See `man execve`,
# specifically the section on "Interpreter scripts" and the one under "NOTES".
script = find_program(f)

custom_target('interpthis',
  input : 'input.txt',
  output : 'output.txt',
  depends : interpreter,
  command : [script, '@INPUT@', '@OUTPUT@'],
  build_by_default : true)
