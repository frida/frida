project('test customtarget')

ct1 = custom_target(
    command: ['generate.py', '@OUTPUT@'],
    output: 'a.py',
)
ct2 = custom_target(
    command: ['generate.py', '@OUTPUT@'],
    output: ['b.py', 'c.py'],
)

test('using_custom_target', ct1)
test('using_custom_target_index', ct2[0])
test('using_custom_target_index_1', ct2[1], should_fail: true)
