# normally run only from run_tests.py or run_project_tests.py
# else do like
# meson build '-Dtestoption=A string with spaces' -Dother_one=true -Dcombo_opt=one -Dprefix=/usr -Dlibdir=lib -Dbackend=ninja -Dwerror=True
project('options')

assert(get_option('testoption') == 'A string with spaces', 'Incorrect value for testoption option.')
assert(get_option('other_one') == true, 'Incorrect value for other_one option.')
assert(get_option('combo_opt') == 'one', 'Incorrect value for combo_opt option.')
