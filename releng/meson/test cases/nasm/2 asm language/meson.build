project('test', 'c')

if not host_machine.cpu_family().startswith('x86')
  assert(not add_languages('nasm', required: false))
  error('MESON_SKIP_TEST: nasm only supported for x86 and x86_64')
endif

if host_machine.system() == 'windows'
  error('MESON_SKIP_TEST: this test asm is not made for Windows')
elif host_machine.system() == 'sunos'
  error('MESON_SKIP_TEST: this test asm is not made for Solaris or illumos')
endif

if meson.backend().startswith('vs')
  error('MESON_SKIP_TEST: VS backend does not recognise NASM yet')
endif

if not add_languages('nasm', required: false)
  nasm = find_program('nasm', 'yasm', required: false)
  assert(not nasm.found())
  error('MESON_SKIP_TEST: nasm not found')
endif

nasm = meson.get_compiler('nasm')

code = '''
SECTION .text
global main
main:
  mov foo,bar ; error: symbol `foo' not defined
'''
assert(not nasm.compiles(code, name: 'Invalid NASM code'))

code = '''
SECTION .text
global main
main:
  mov eax,1
'''
assert(nasm.compiles(code, name: 'Valid NASM code'))

config_file = configure_file(
  output: 'config.asm',
  configuration: {'HELLO': 0},
  output_format: 'nasm',
)

cc = meson.get_compiler('c')
link_args = cc.get_supported_link_arguments(['-no-pie'])

exe = executable('hello', 'hello.asm',
  nasm_args: '-DFOO',
  link_args: link_args,
)
test('hello', exe)

#Test whether pthread dependency gets filtered out
threads = dependency('threads')

exe2 = executable('hello_w_threads', 'hello.asm',
  config_file,
  nasm_args: '-DFOO',
  link_args: link_args,
  dependencies: [threads]
)

test('hello_w_threads', exe2)
