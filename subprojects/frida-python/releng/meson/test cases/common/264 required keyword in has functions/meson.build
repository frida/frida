project('required keyword in has functions', 'c')

cc = meson.get_compiler('c')
opt = get_option('opt')

cc.has_function('printf', prefix : '#include<stdio.h>', required : true)
cc.has_type('time_t', prefix : '#include<time.h>', required : true)
cc.has_member('struct tm', 'tm_sec', prefix : '#include<time.h>', required : true)
cc.has_members('struct tm', ['tm_sec', 'tm_min'], prefix : '#include<time.h>', required : true)
cc.has_header('time.h', required : true)
cc.has_header_symbol('time.h', 'time', required : true)

assert(not cc.has_function('printf', prefix : '#include<stdio.h>', required : opt))
assert(not cc.has_type('time_t', prefix : '#include<time.h>', required : opt))
assert(not cc.has_member('struct tm', 'tm_sec', prefix : '#include<time.h>', required : opt))
assert(not cc.has_members('struct tm', ['tm_sec', 'tm_min'], prefix : '#include<time.h>', required : opt))
assert(not cc.has_header('time.h', required : opt))
assert(not cc.has_header_symbol('time.h', 'time', required : opt))

# compiler.has_argument
if cc.get_id() == 'msvc'
  is_arg = '/O2'
else
  is_arg = '-O2'
endif
cc.has_argument(is_arg, required: true)
assert(not cc.has_argument(is_arg, required: opt))

# compiler.has_multi_arguments
if cc.get_id() == 'gcc'
  pre_arg = '-Wformat'
  arg = '-Werror=format-security'
  cc.has_multi_arguments([pre_arg, arg], required: true)
  assert(not cc.has_multi_arguments(pre_arg, arg, required: opt))
endif

# compiler.has_link_argument
if cc.get_argument_syntax() == 'msvc'
  is_arg = '/OPT:REF'
else
  is_arg = '-Wl,-L/tmp'
endif
cc.has_link_argument(is_arg, required: true)
assert(not cc.has_link_argument(is_arg, required: opt))

# compiler.has_function_attribute
if not ['pgi', 'msvc', 'clang-cl', 'intel-cl'].contains(cc.get_id())
  a = 'aligned'
  cc.has_function_attribute(a, required: true)
  assert(not cc.has_function_attribute(a, required: opt))
endif

testcase expect_error('''compiler.has_function keyword argument 'required' was of type str but should have been one of: bool, UserFeatureOption''')
  cc.has_function('printf', required : 'not a bool')
endtestcase

testcase expect_error('''C function 'asdfkawlegsdiovapfjhkr' not usable''')
  cc.has_function('asdfkawlegsdiovapfjhkr', required : true)
endtestcase

testcase expect_error('''C header 'asdfkawlegsdiovapfjhkr.h' not found''')
  cc.has_header('asdfkawlegsdiovapfjhkr.h', required : true)
endtestcase

testcase expect_error('''C symbol time_not_found not found in header time.h''')
  cc.has_header_symbol('time.h', 'time_not_found', required : true)
endtestcase
