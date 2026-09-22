project('default library', 'cpp')

flib = library('ef', 'ef.cpp')
exe = executable('eftest', 'eftest.cpp', link_with : flib)
test('eftest', exe)

# Same as above, but using build_target()
flib2 = build_target('ef2', 'ef.cpp', target_type: 'library')
exe2 = executable('eftest2', 'eftest.cpp', link_with : flib2)
test('eftest2', exe2)
