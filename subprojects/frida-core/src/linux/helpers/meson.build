project('frida-linux-helpers', 'c',
  default_options: [
    'c_std=gnu99',
    'optimization=s',
    'b_ndebug=true',
    'strip=true',
  ],
)

cc = meson.get_compiler('c')

objcopy = find_program('objcopy')

extra_flags = cc.get_supported_arguments([
  '-fvisibility=hidden',
  '-fno-function-sections',
  '-fno-data-sections',
  '-fno-optimize-strlen',
  '-fno-asynchronous-unwind-tables',
  '-mno-memcpy',
])
if host_machine.cpu_family() == 'arm'
  # Cancel out -mthumb, as we want maximum portability.
  extra_flags += '-marm'
endif

if host_machine.cpu_family() == 'x86'
  extra_flags += '-fno-stack-protector'
endif

objcopy_flags = [
  '-O', 'binary',
  '-j', '.payload',
]

helper_lds = meson.current_source_dir() / 'helper.lds'

extra_link_args = [
  '-nostdlib',
  '-Wl,-T,' + helper_lds,
]
if host_machine.cpu_family() == 'arm'
  extra_link_args += run_command(cc, '-print-libgcc-file-name', check: false).stdout().strip()
endif


bootstrapper_sources = [
  'bootstrapper.c',
  'elf-parser.c',
]

bootstrapper = shared_module('bootstrapper', bootstrapper_sources,
  name_prefix: '',
  c_args: [
    '-DNOLIBC_DISABLE_START',
    '-DNOLIBC_IGNORE_ERRNO',
    '-include', 'ext/linux/tools/include/nolibc/nolibc.h',
    '-ffreestanding',
  ] + extra_flags,
  link_args: extra_link_args,
  link_depends: [helper_lds],
)
custom_target('bootstrapper-bin',
  input: [bootstrapper],
  output: ['bootstrapper.bin'],
  command: [objcopy, objcopy_flags, '@INPUT0@', '@OUTPUT0@'],
  install_dir: 'share' / 'frida-helpers',
  install: true,
)
test('bootstrapper', executable('test-bootstrapper', bootstrapper_sources,
  c_args: [
    '-DBUILDING_TEST_PROGRAM',
  ],
))

loader = shared_module('loader', ['loader.c', 'syscall.c'],
  name_prefix: '',
  c_args: extra_flags,
  link_args: extra_link_args,
  link_depends: [helper_lds],
)
custom_target('loader-bin',
  input: [loader],
  output: ['loader.bin'],
  command: [objcopy, objcopy_flags, '@INPUT0@', '@OUTPUT0@'],
  install_dir: 'share' / 'frida-helpers',
  install: true,
)

if host_machine.system() == 'android'
  zymbiote_flags = []
  if host_machine.cpu_family() == 'aarch64'
    zymbiote_flags += '-mbranch-protection=bti'
  endif
  zymbiote = shared_module('zymbiote', ['zymbiote.c'],
    name_prefix: '',
    name_suffix: 'elf',
    c_args: zymbiote_flags + extra_flags,
    link_args: extra_link_args,
    link_depends: [helper_lds],
    install_dir: 'share' / 'frida-helpers',
    install: true,
  )
endif
