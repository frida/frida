project('options', 'c', meson_version : '>= 1.0.0')

if get_option('testoption') != 'optval'
  error('Incorrect value to test option')
endif

if get_option('other_one') != false
  error('Incorrect value to boolean option.')
endif

if get_option('combo_opt') != 'combo'
  error('Incorrect value to combo option.')
endif

if get_option('array_opt') != ['one', 'two']
  message(get_option('array_opt'))
  error('Incorrect value for array option')
endif

# If the default changes, update test cases/unit/13 reconfigure
if get_option('b_lto') != false
  error('Incorrect value in base option.')
endif

if get_option('includedir') != 'include'
  error('Incorrect value in builtin option.')
endif

if get_option('integer_opt') != 3
  error('Incorrect value in integer option.')
endif

if get_option('neg_int_opt') != -3
  error('Incorrect value in negative integer option.')
endif

if get_option('CaseSenSiTivE') != 'Some CAPS'
  error('Incorrect value in mixed caps option.')
endif

if get_option('CASESENSITIVE') != 'ALL CAPS'
  error('Incorrect value in all caps option.')
endif

assert(get_option('wrap_mode') == 'default', 'Wrap mode option is broken.')
assert(get_option('boolean_string') == false)
assert(get_option('boolean_string2') == true)
assert(get_option('integer_string') == 42)

testcase expect_error('Invalid option name \'..invalid\'')
  get_option('..invalid')
endtestcase

testcase expect_error('Invalid option name \'this.is.also.invalid\'')
  get_option('this.is.also.invalid')
endtestcase
