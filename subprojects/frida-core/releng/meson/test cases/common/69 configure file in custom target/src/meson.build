custom_target('thing',
output : 'final.dat',
input : cfile,
command : [find_program('mycompiler.py'), '@INPUT@', '@OUTPUT@'])

# Test usage of a `configure_file` as part of the command list
py3 = find_program('python3', required : false)
if not py3.found()
  # Maybe 'python' is Python 3
  py3 = find_program('python')
endif

compiler = configure_file(input : 'mycompiler.py',
  output : 'mycompiler2.py',
  copy: true)

custom_target('thing2',
output : 'final2.dat',
input : cfile,
command : [py3, compiler, '@INPUT@', '@OUTPUT@'])
