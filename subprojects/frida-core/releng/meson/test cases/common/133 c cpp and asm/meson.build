project('c cpp and asm', 'c', 'cpp')

cpu = host_machine.cpu_family()
cc = meson.get_compiler('c')

supported_cpus = ['arm', 'x86', 'x86_64']

if not supported_cpus.contains(cpu)
  error('MESON_SKIP_TEST unsupported cpu:' + cpu)
endif

if meson.get_compiler('c').get_argument_syntax() == 'msvc'
  error('MESON_SKIP_TEST MSVC can\'t compile assembly')
endif

if cc.symbols_have_underscore_prefix()
  add_project_arguments('-DMESON_TEST__UNDERSCORE_SYMBOL', language: 'c')
endif

test('test-c-asm', executable('c-asm', ['main.c', 'retval-' + cpu + '.S']))
test('test-cpp-asm', executable('cpp-asm', ['main.cpp', 'retval-' + cpu + '.S']))
test('test-c-cpp-asm', executable('c-cpp-asm', ['somelib.c', 'main.cpp', 'retval-' + cpu + '.S']))
test('test-cpp-c-asm', executable('cpp-c-asm', ['main.cpp', 'somelib.c', 'retval-' + cpu + '.S']))
