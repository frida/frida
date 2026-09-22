project('subproject-sandbox-violation')

sub1_d = subproject('subproj1').get_variable('d')
sub1_mustfail = sub1_d.get_variable('dir') / '..' / 'file.txt'

sub2_d = subproject('subproj2').get_variable('d')
sub2_mustfail = sub2_d.get_variable('dir') / 'file.txt'

main_d = declare_dependency(
    variables: [
        'dir=@0@'.format(meson.current_source_dir()),
    ]
)
main_mustfail = main_d.get_variable('dir') / 'subprojects/subproj3/file.txt'

if get_option('failmode') == 'parent-dir'
    mustfail = sub1_mustfail
elif get_option('failmode') == 'not-installed'
    mustfail = sub2_mustfail
elif get_option('failmode') == 'root-subdir'
    mustfail = main_mustfail
endif

custom_target(
    'mustfail',
    input: mustfail,
    output: 'file.txt',
    command: [
        'python3', '-c',
        'import os; shutil.copy(sys.argv[1], sys.argv[2])',
        '@INPUT@',
        '@OUTPUT@'
    ],
)
