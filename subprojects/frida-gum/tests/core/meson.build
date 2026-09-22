if host_os == 'macos'
  subdir('swiftapiresolver')
endif

core_sources = [
  'tls.c',
  'controlflowgraph.c',
  'cloak.c',
  'memory.c',
  'process.c',
  'threadregistry.c',
  'moduleregistry.c',
  'symbolutil.c',
  'apiresolver.c',
  'backtracer.c',
  'interceptor.c',
  'interceptor-callbacklistener.c',
  'interceptor-functiondatalistener.c',
  'memoryaccessmonitor.c',
  'arch-x86' / 'x86writer.c',
  'arch-x86' / 'x86relocator.c',
  'arch-arm' / 'armwriter.c',
  'arch-arm' / 'armrelocator.c',
  'arch-arm' / 'thumbwriter.c',
  'arch-arm' / 'thumbrelocator.c',
  'arch-arm64' / 'arm64writer.c',
  'arch-arm64' / 'arm64relocator.c',
]

core_c_args = []

if host_os_family == 'darwin'
  core_sources += [
    'interceptor-darwin.c',
    'unwindbroker-darwin.c',
  ]

  mapper_unwind_target = shared_library('gum-tests-mapper-unwind-target',
    'mapper-unwind-target.c',
  )
  core_c_args += '-DGUM_TESTS_MAPPER_UNWIND_TARGET="' + mapper_unwind_target.full_path() + '"'
endif

if host_os == 'android'
  core_sources += [
    'interceptor-android.c',
  ]
endif

if host_arch in ['x86', 'x86_64']
  core_sources += [
    'arch-x86' / 'stalker-x86.c',
  ]
  if host_os == 'macos'
    core_sources += [
      'arch-x86' / 'stalker-x86-macos.m',
    ]
  endif
endif

if host_arch == 'arm'
  core_sources += [
    'arch-arm' / 'interceptor-arm.c',
    'arch-arm' / 'stalker-arm.c',
  ]
endif

if host_arch == 'arm64'
  core_sources += [
    'arch-arm64' / 'interceptor-arm64.c',
    'arch-arm64' / 'stalker-arm64.c',
  ]
  if host_os_family == 'darwin'
    core_sources += [
      'arch-arm64' / 'stalker-arm64-darwin.m',
    ]
  endif
  if host_os == 'macos'
    core_sources += [
      'arch-arm64' / 'stalker-arm64-macos.m',
    ]
  endif
endif

core_link_with = []

if host_os_family == 'linux' and host_arch in ['x86', 'x86_64', 'arm64']
  gum_tests_core_exceptions = static_library('gum-tests-core-exceptions',
    ['arch-x86/stalker-x86-exceptions.cpp'],
    include_directories: test_incdirs,
    dependencies: [gum_dep],
    override_options: ['cpp_eh=default'],
  )
  core_link_with += gum_tests_core_exceptions
endif

gum_tests_core = static_library('gum-tests-core', core_sources,
  c_args: core_c_args,
  include_directories: test_incdirs,
  dependencies: [gum_dep, lzma_dep],
  link_with: core_link_with,
)

if host_os_family == 'darwin'
  symlist = 'mapper.symbols'

  mapper_test_unsigned = executable('mapper-test-unsigned', 'mapper.c',
    dependencies: [gum_dep],
    link_args: ['-Wl,-exported_symbols_list,' + (meson.current_source_dir() / symlist)],
    link_depends: [symlist],
  )

  custom_target('mapper-test-signed',
    input: [
      mapper_test_unsigned,
      'mapper.xcent',
    ],
    output: 'mapper-test',
    command: [
      files('../sign.sh'),
      host_os,
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )
endif
