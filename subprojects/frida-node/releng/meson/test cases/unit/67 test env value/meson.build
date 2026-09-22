project('test env value')

testpy = find_program('test.py')

val = ['1', '2', '3']
foreach v: val
    test('check env', testpy, args: [v], env: {'TEST_VAR': v})
endforeach
test('check env', testpy, args: ['4'], env: environment({'TEST_VAR': '4'}))
test('check env', testpy, args: ['5'], env: environment(['TEST_VAR=5']))
