project('timeout', 'c')

# This creates a test that times out. It is a manual test
# because currently there is no test suite for test that are expected
# to fail during unit test phase.

exe = executable('sleepprog', 'sleepprog.c')
test('timeout', exe, timeout : 1)
