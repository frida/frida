project('testcase clause')

# To make sure unreachable code is not executed.
unreachable = true

# Verify assertion exception gets catched and dropped.
testcase expect_error('Assert failed: false')
  assert(false)
  unreachable = false
endtestcase
assert(unreachable)

# The inner testcase raises an exception because it did not receive the expected
# error message. The outer testcase catches the inner testcase exception and
# drop it.
testcase expect_error('Expecting error \'something\' but got \'Assert failed: false\'')
  testcase expect_error('something')
    assert(false)
    unreachable = false
  endtestcase
  unreachable = false
endtestcase
assert(unreachable)

# The inner testcase raises an exception because it did not receive an
# exception. The outer testcase catches the inner testcase exception and
# drop it.
testcase expect_error('Expecting an error but code block succeeded')
  testcase expect_error('something')
    reached = true
  endtestcase
  unreachable = false
endtestcase
assert(reached)
assert(unreachable)

message('all good')
