project('test format string')

# Test all supported types in message(), format(), f-string.
foreach t : [get_option('opt'), 42, true, false, 'str', ['list'], {'dict': 'value'}]
  message(t, '@0@'.format(t), f'@t@', [t], {'key': t})
endforeach

# Deprecated but should work with str.format().
env = environment()
message('@0@'.format(env))

# Should fail with f-string and message()
error_msg = 'Value other than strings, integers, bools, options, dictionaries and lists thereof.'
testcase expect_error('message(): ' + error_msg)
  message(env)
endtestcase

testcase expect_error('f-string: ' + error_msg)
  message(f'@env@')
endtestcase
