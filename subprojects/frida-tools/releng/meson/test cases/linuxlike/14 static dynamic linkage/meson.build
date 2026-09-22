project('static dynamic', 'c')

# Solaris does not ship static libraries
if host_machine.system() == 'sunos'
  has_static = false
else
  has_static = true
endif

cc = meson.get_compiler('c')

z_default = cc.find_library('z')
if has_static
  z_static = cc.find_library('z', static: true)
endif
z_dynamic = cc.find_library('z', static: false)

exe_default = executable('main_default', 'main.c', dependencies: [z_default])
if has_static
  exe_static = executable('main_static', 'main.c', dependencies: [z_static])
endif
exe_dynamic = executable('main_dynamic', 'main.c', dependencies: [z_dynamic])

test('test default', exe_default)
if has_static
  test('test static', exe_static)
endif
test('test dynamic', exe_dynamic)

if has_static
  test('verify static linking', find_program('verify_static.py'),
      args: ['--platform=' + host_machine.system(), exe_static.full_path()])
endif
test('verify dynamic linking', find_program('verify_static.py'),
    args: ['--platform=' + host_machine.system(), exe_dynamic.full_path()],
    should_fail: true)
