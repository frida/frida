project('testprovider','c')

deplib = dependency('libtestprovider', static:true)

dprovidertest = executable('dprovidertest',
    files('./receiver.c'),
    dependencies:[deplib],
    c_args:['-Wall','-Werror'],
)

test('testprovider',dprovidertest)
