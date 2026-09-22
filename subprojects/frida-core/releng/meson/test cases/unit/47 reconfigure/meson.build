project('test-reconfigure', 'c')

message('opt1 ' + get_option('opt1'))
message('opt2 ' + get_option('opt2'))
message('opt3 ' + get_option('opt3'))
message('opt4 ' + get_option('opt4'))

exe = executable('test1', 'main.c')
test('test1', exe)

sub1 = subproject('sub1')
