project('nasm config file', 'c')

if host_machine.cpu_family() == 'x86' and host_machine.system() == 'windows'
  asm_format = 'win32'
elif host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'windows'
  asm_format = 'win64'
elif host_machine.cpu_family() == 'x86' and host_machine.system() == 'linux'
  asm_format = 'elf32'
elif host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'linux'
  asm_format = 'elf64'
else
  error('MESON_SKIP_TEST: skipping test on this platform')
endif

nasm = find_program('nasm', required: false)

if not nasm.found()
  error('MESON_SKIP_TEST: nasm not available')
endif

conf = configuration_data()

conf.set('HELLO', 0)

asm_gen = generator(nasm,
  output : '@BASENAME@.o',
  arguments : [
    '-f', asm_format,
    '-i', meson.current_source_dir() + '/',
    '-i', join_paths(meson.current_source_dir(), ''),
    '-P', join_paths(meson.current_build_dir(), 'config.asm'),
    '@INPUT@',
    '-o', '@OUTPUT@'])


config_file = configure_file(
  output: 'config.asm',
  configuration: conf,
  output_format: 'nasm',
)

cc = meson.get_compiler('c')
link_args = cc.get_supported_link_arguments(['-no-pie'])

exe = executable('hello', asm_gen.process('hello.asm'),
  link_args: link_args,
)

test('test-nasm-configure-file', exe)

exe2 = executable('hello2', objects : exe.extract_all_objects(),
  link_args: link_args,
)

test('test-nasm-extract-all-objects', exe2)
