project('curses', 'c')

curses = dependency('curses', required: false, method : get_option('method'), version : '>= 0')
if not curses.found()
  error('MESON_SKIP_TEST: Curses library not found')
endif

exec = executable('basic', 'main.c', dependencies: curses)
# didn't run the test because in general graphics fail on CI

# this should fail
not_found = dependency('curses', required: false, method : get_option('method'), version : '> 1000000')
assert(not_found.found() == false)
