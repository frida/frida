project('valatest', 'vala', 'c')

valadeps = [dependency('glib-2.0'), dependency('gobject-2.0')]
valafiles = files(
    'prog.vala',
    'Test.vala',
    'Subdir/Test.vala',
    'Subdir/Subdir2/Test.vala',
    'Subdir2/Test.vala',
)

e = executable('multidir_prog', valafiles, dependencies : valadeps)
test('valatest', e)
