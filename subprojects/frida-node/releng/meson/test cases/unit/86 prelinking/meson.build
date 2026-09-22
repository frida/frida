project('prelinking', 'c')

liba = static_library('prelinked', 'file1.c', 'file2.c', 'file3.c', 'file4.c',
    prelink: true)
exe = executable('testprog', 'main.c',
    link_with: liba)
test('prelinked', exe)
