project('link_depends_indexed_custom_target', 'c')

cmd = find_program('make_file.py')

dep_files = custom_target('gen_dep',
        command: [cmd, '@OUTPUT@'],
        output: ['dep_file1', 'dep_file2'])

exe = executable('foo', 'foo.c',
        link_depends: dep_files[1],
        c_args: ['-DDEPFILE="' + dep_files[0].full_path()+ '"'])

check_arch = find_program('check_arch.py')
custom_target('check-arch',
        command: [check_arch, exe, host_machine.cpu_family(), '@OUTPUT@'],
        build_by_default: true,
        output: 'dummy.txt')

# check that dep_file1 exists, which means that link_depends target ran
test('runtest', exe)
