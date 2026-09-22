project('testsetups', 'c')

vg = find_program('valgrind')

cc = meson.get_compiler('c')
# clang 14 uses dwarf 5, and valgrind 3.19 GIT does not support this
if cc.get_id() == 'clang' and cc.version().version_compare('>=14') and \
    vg.version().version_compare('<3.20')
  add_project_arguments('-gdwarf-4', language: 'c')
endif

# This is only set when running under Valgrind test setup.
env = environment()
env.set('TEST_ENV', '1')

add_test_setup('valgrind',
  exe_wrapper : [vg, '--error-exitcode=1', '--leak-check=full'],
  timeout_multiplier : 100,
  env : env)

buggy = executable('buggy', 'buggy.c', 'impl.c')
test('Test buggy', buggy, suite: ['buggy'])

envcheck = find_program('envcheck.py')
test('test-env', envcheck)

add_test_setup('empty')
add_test_setup('onlyenv', env : env)
add_test_setup('onlyenv2', env : 'TEST_ENV=1')
add_test_setup('onlyenv3', env : ['TEST_ENV=1'])
add_test_setup('wrapper', exe_wrapper : [vg, '--error-exitcode=1'])
add_test_setup('timeout', timeout_multiplier : 20)
add_test_setup('good', exclude_suites : 'buggy')
