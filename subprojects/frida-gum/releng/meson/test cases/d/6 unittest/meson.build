project('D Unittests', 'd')

e = executable('dapp', ['app.d', 'second_unit.d'], install : true)
test('dapp_run', e, should_fail: true)

e_test = executable('dapp_test', ['app.d', 'second_unit.d'],
    d_unittest: true)
test('dapp_test', e_test)
