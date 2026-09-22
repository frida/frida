project('generated assembly', 'c')

cc = meson.get_compiler('c')

if ['msvc', 'intel-cl'].contains(cc.get_id())
  error('MESON_SKIP_TEST: assembly files cannot be compiled directly by the compiler')
endif

if meson.backend() == 'xcode'
  error('MESON_SKIP_TEST: asm not supported with the Xcode backend. Patches welcome.')
endif


crt_workaround = []
if cc.get_linker_id() == 'lld-link'
  # It seems that when building without a .c file, lld-link.exe
  # misses the fact that it needs to include the c runtime to
  # make a working .dll. So here we add an empty .c file to easily
  # pull in crt.
  crt_workaround += 'empty.c'
  if host_machine.cpu_family() == 'x86'
    # x86 assembly needs manual annotation to be compatible with
    # Safe Exception Handlers (?) This assembly doesn't have such
    # annotation, so just disable the feature.
    add_project_link_arguments('/SAFESEH:NO', language : 'c')
  endif
endif

cpu = host_machine.cpu_family()
supported_cpus = ['arm', 'x86', 'x86_64']

if not supported_cpus.contains(cpu)
  error('MESON_SKIP_TEST: unsupported cpu family: ' + cpu)
endif

if cc.get_id() == 'clang-cl' and cc.version().version_compare('< 12.0.0') and cpu == 'arm'
  # https://reviews.llvm.org/D89622
  error('MESON_SKIP_TEST: arm debug symbols not supported in clang-cl < 12.0.0')
endif

if cc.symbols_have_underscore_prefix()
  add_project_arguments('-DMESON_TEST__UNDERSCORE_SYMBOL', language : 'c')
endif

copy = find_program('copyfile.py')
output = 'square-@0@.S'.format(cpu)
input = output + '.in'

copygen = generator(copy,
  arguments : ['@INPUT@', '@OUTPUT@'],
  output : '@BASENAME@')

l = library('square-gen', crt_workaround + [copygen.process(input)],
  vs_module_defs: 'square.def')

test('square-gen-test', executable('square-gen-test', 'main.c', link_with : l))

copyct = custom_target('square',
  input : input,
  output : output,
  command : [copy, '@INPUT@', '@OUTPUT@'])

l = library('square-ct', crt_workaround + [copyct],
  vs_module_defs: 'square.def')

test('square-ct-test', executable('square-ct-test', 'main.c', link_with : l))
