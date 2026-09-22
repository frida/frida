project('long-stderr', 'c')

dumper = executable('dumper', 'dumper.c')
test('dump-test', dumper, timeout: 60)
test('dump-test-TAP', dumper, protocol : 'tap', timeout: 60)
