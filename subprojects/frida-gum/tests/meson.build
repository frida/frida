runner_sources = [
  'gumtest.c',
  'testutil.c',
  'stubs' / 'dummyclasses.c',
  'stubs' / 'fakebacktracer.c',
  'stubs' / 'fakeeventsink.c',
  'stalkerdummychannel.c',
  'lowlevelhelpers.c',
]

test_incdirs = [
  include_directories('.'),
  include_directories('stubs'),
]

subdir('core')
subdir('heap')
subdir('prof')
if have_gumpp
  subdir('gumpp')
else
  gum_tests_gumpp = []
endif
if have_gumjs
  subdir('gumjs')
else
  gum_tests_gumjs = []
endif

subdir('data')

runner_name = 'gum-tests'
if host_os_family == 'darwin'
  runner_name = 'gum-tests-unsigned'
endif

link_targets = [
  gum_tests_core,
  gum_tests_heap,
  gum_tests_prof,
  gum_tests_gumpp,
  gum_tests_gumjs,
]

extra_test_deps = [lzma_dep]
extra_link_args = []
extra_link_depends = []

if have_gumjs
  extra_test_deps += [gumjs_dep]
endif

force_cpp_linking = have_gumpp or v8_dep.found()

# We have tests for Stalker C++ exception handling
if host_os_family == 'linux' and host_arch in ['x86', 'x86_64']
  force_cpp_linking = true
endif

if force_cpp_linking
  if host_os_family == 'darwin'
    # Work around Meson bug: https://github.com/mesonbuild/meson/issues/1766
    runner_sources += ['dummy.mm']
  else
    runner_sources += ['dummy.cpp']
  endif
endif

if host_os_family == 'windows'
  fs = import('fs')
  extra_link_depends += fs.copyfile(dbghelp_dll)
  extra_link_depends += fs.copyfile(symsrv_dll)
endif

if host_os_family == 'darwin'
  symlist = 'gum-tests.symbols'
  extra_link_args += ['-Wl,-exported_symbols_list,' + (meson.current_source_dir() / symlist)]
  extra_link_depends += [symlist]

  subdir('stubs' / 'objc')
  link_targets += objcdummyclass
elif host_os_family != 'windows'
  if host_os == 'android'
    symscript = 'gum-tests-android.version'
  elif host_os == 'freebsd'
    symscript = 'gum-tests-freebsd.version'
  else
    symscript = 'gum-tests.version'
  endif
  extra_link_args += ['-Wl,--version-script,' + (meson.current_source_dir() / symscript)]
  extra_link_depends += [symscript]
endif

if host_os == 'macos'
  extra_link_args += ['-Wl,-framework,AppKit']
endif

if host_os in ['ios', 'watchos', 'tvos']
  extra_link_args += ['-Wl,-framework,Foundation']
endif

raw_runner = executable(runner_name, runner_sources + [test_data_stamp],
  link_with: link_targets,
  dependencies: [gum_dep, gum_heap_dep, gum_prof_dep, tls_provider_dep, threads_dep] + extra_test_deps,
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  export_dynamic: true,
)

if host_os_family == 'darwin'
  runner = custom_target('gum-tests-signed',
    input: [
      raw_runner,
      'gum-tests.xcent',
    ],
    output: 'gum-tests',
    command: [
      files('sign.sh'),
      host_os,
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )
else
  runner = raw_runner
endif

test('gum', python,
  args: [files('run.py'), runner.full_path()],
  timeout: 120,
)
