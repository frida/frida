project('find program', 'c')

# Test that we can find native windows executables
find_program('cmd')
find_program('cmd.exe')

# Test that a script file with an extension can be found
ext = find_program('test-script-ext.py')
test('ext', ext)
# Test that a script file without an extension can be found
prog = find_program('test-script')
test('script', prog)
