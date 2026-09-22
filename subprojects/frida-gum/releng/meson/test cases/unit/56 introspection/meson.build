project('introspection', ['c', 'cpp'], version: '1.2.3', default_options: ['cpp_std=c++11', 'buildtype=debug'])

dep1 = dependency('threads')
dep2 = dependency('zlib', required: false)
dep3 = dependency('bugDep1', required: get_option('test_opt1'))

b1 = get_option('test_opt1')
b2 = get_option('test_opt2')
test_bool = b1 or b2
test_bool = b1 and b2
test_bool = not test_bool

set_variable('list_test_plusassign', [])
list_test_plusassign += ['bugs everywhere']
dict_test = {list_test_plusassign[0]: 'even more bugs'}

if not true
  vers_str = '<=99.9.9'
  dependency('somethingthatdoesnotexist', required: true, version: '>=1.2.3')
  dependency('look_i_have_a_fallback', version: ['>=1.0.0', vers_str], fallback: ['oh_no', 'the_subproject_does_not_exist'])
endif

subdir('sharedlib')
subdir('staticlib')

var1 = '1'
var2 = 2.to_string()
var3 = 'test3'

var4 = f'test @var1@ string' # TODO: Actually implement fstrings

cus1 = custom_target('custom target test 1', output: 'file2', input: 'cp.py',
                     command: [find_program('cp.py'), '@INPUT@', '@OUTPUT@'])
cus2 = custom_target('custom target test 2', output: 'file3', input: cus1,
                     command: [find_program('cp.py'), '@INPUT@', '@OUTPUT@'])

t1 = executable('test' + var1, ['t1.cpp'], link_with: [sharedlib], install: not false, build_by_default: get_option('test_opt2'))
t2 = executable('test@0@'.format('@0@'.format(var2)), sources: ['t2.cpp'], link_with: [staticlib])
t3 = executable(var3, 't3.cpp', link_with: [sharedlib, staticlib], dependencies: [dep1])

cus3 = custom_target('custom target test 3', output: 'file4', input: t3,
                     command: [find_program('cp.py'), '@INPUT@', '@OUTPUT@'])

### BEGIN: Test inspired by taisei: https://github.com/taisei-project/taisei/blob/master/meson.build#L293
systype = '@0@'.format(host_machine.system())
systype = '@0@, @1@, @2@'.format(systype, host_machine.cpu_family(), host_machine.cpu())
message(systype)
### END: Test inspired by taisei

# Minimal code version to produce bug #5376
# Code inspired by https://github.com/mesa3d/mesa/blob/974c4d679c23373dbed386c696e3e3bc1bfa23ae/meson.build#L1341-L1347
osmesa_lib_name = 'OSMesa'
osmesa_bits = '8'
osmesa_lib_name = osmesa_lib_name + osmesa_bits
message(osmesa_lib_name) # Infinite recursion gets triggered here when the parameter osmesa_lib_name is resolved

test('test case 1', t1)
test('test case 2', t2, depends: t3)
benchmark('benchmark 1', t3, args: [cus1, cus2, cus3])

### Stuff to test the AST JSON printer
foreach x : ['a', 'b', 'c']
  if x == 'a'
    message('a')
  elif x == 'b'
    message('a')
  else
    continue
  endif
  break
  continue
endforeach
