project('c++ and assembly test')
add_languages('cpp')

if meson.backend() == 'xcode'
  error('MESON_SKIP_TEST: asm not supported with the Xcode backend. Patches welcome.')
endif

cpp = meson.get_compiler('cpp')
cpu = host_machine.cpu_family()

supported_cpus = ['arm', 'x86', 'x86_64']

if not supported_cpus.contains(cpu)
  error('MESON_SKIP_TEST unsupported cpu:' + cpu)
endif

if cpp.symbols_have_underscore_prefix()
  add_project_arguments('-DMESON_TEST__UNDERSCORE_SYMBOL', language : 'cpp')
endif

sources = ['trivial.cc']
# If the compiler cannot compile assembly, don't use it
if not ['msvc', 'clang-cl', 'intel-cl'].contains(meson.get_compiler('cpp').get_id())
  sources += ['retval-' + cpu + '.S']
  cpp_args = ['-DUSE_ASM']
  message('Using ASM')
else
  cpp_args = ['-DNO_USE_ASM']
endif

exe = executable('trivialprog', sources,
  cpp_args : cpp_args)
test('runtest', exe)
