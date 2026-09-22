project('link_depends_custom_target', 'c')

cmd = find_program('make_file.py')

dep_file = custom_target('gen_dep',
        command: [cmd, '@OUTPUT@'],
        output: 'dep_file')

exe = executable('foo', 'foo.c',
        link_depends: dep_file,
        c_args: ['-DDEPFILE="' + dep_file.full_path()+ '"'])

# check that dep_file exists, which means that link_depends target ran
test('runtest', exe)
