project('subproject test suites', 'c')

sub1 = executable('sub1', 'sub1.c')
sub2 = executable('sub2', 'sub2.c')

test('sub1', sub1)
test('sub2', sub2, suite : 'suite2')
