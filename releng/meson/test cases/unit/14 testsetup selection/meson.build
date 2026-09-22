project('main', 'c')

main = executable('main', 'main.c')
test('Test main', main)

add_test_setup('worksforall')
add_test_setup('missingfromfoo')

subproject('foo')
subproject('bar')
