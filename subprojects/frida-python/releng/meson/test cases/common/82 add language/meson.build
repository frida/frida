project('add language', 'c')

test('C', executable('cprog', 'prog.c'))

assert(add_languages('cpp', native: false), 'Add_languages returned false on success')
assert(not add_languages('klingon', required : false), 'Add_languages returned true on failure.')

test('C++', executable('cppprog', 'prog.cc'))

add_languages('c', native: false)
