project('vala and asm', 'vala', 'c')

cpu = host_machine.cpu_family()
cc = meson.get_compiler('c')

supported_cpus = ['arm', 'x86', 'x86_64']

if not supported_cpus.contains(cpu)
  error('MESON_SKIP_TEST unsupported cpu:' + cpu)
endif

if meson.get_compiler('c').get_id() == 'msvc'
  error('MESON_SKIP_TEST MSVC can\'t compile assembly')
endif

if cc.symbols_have_underscore_prefix()
  add_project_arguments('-DMESON_TEST__UNDERSCORE_SYMBOL', language: 'c')
endif

valadeps = [dependency('glib-2.0'), dependency('gobject-2.0')]
e = executable('vala-asm', ['prog.vala', 'retval-' + cpu + '.S'],
               dependencies: valadeps)
test('test-vala-asm', e)
