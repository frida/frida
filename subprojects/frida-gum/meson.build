project('frida-gum', 'c', 'cpp',
  version: run_command(find_program('python3'), files('tools' / 'detect-version.py'),
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.1.0',
  default_options: [
    'c_std=gnu99,c99',
  ],
)

python = import('python').find_installation()

releng = meson.global_source_root() / 'releng'
if not import('fs').exists(releng)
  releng = meson.project_source_root() / 'releng'
endif

frida_version = get_option('frida_version')
if frida_version == ''
  frida_version = run_command(python, releng / 'frida_version.py', meson.project_source_root(), check: true).stdout().strip()
endif

api_version = '1.0'

host_os_family = host_machine.system()
if host_os_family == 'android'
  host_os_family = 'linux'
endif
host_os = host_machine.subsystem().split('-')[0]
host_config = ''
if host_machine.subsystem().contains('-')
  host_config = '-' + host_machine.subsystem().split('-')[1]
endif
if host_machine.cpu_family() == 'arm'
  host_arch = 'arm'
  host_abi = 'arm'
elif host_machine.cpu_family() == 'aarch64'
  host_arch = 'arm64'
  host_abi = 'arm64'
elif host_machine.cpu_family() == 'mips'
  host_arch = 'mips'
  if host_machine.endian() == 'little'
    host_abi = 'mipsel'
  else
    host_abi = 'mips'
  endif
elif host_machine.cpu_family() == 'mips64'
  host_arch = 'mips64'
  if host_machine.endian() == 'little'
    host_abi = 'mips64el'
  else
    host_abi = 'mips64'
  endif
else
  host_arch = host_machine.cpu_family()
  host_abi = host_arch
endif

languages = ['c', 'cpp']
if host_os_family == 'darwin'
  languages += ['objc', 'objcpp']
  add_languages('objc', 'objcpp', native: false)
endif

cc = meson.get_compiler('c')

if cc.get_argument_syntax() == 'msvc' and host_arch == 'arm64'
  add_languages('masm')
endif

frida_component_cflags = []
ndebug = get_option('b_ndebug')
optimize_for_prod = ndebug == 'true' or (ndebug == 'if-release' and not get_option('debug'))
if optimize_for_prod
  frida_component_cflags += [
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CHECKS',
    '-DG_DISABLE_CAST_CHECKS',
  ]
endif

if host_arch == 'arm'
  is_hardfloat_src = '''
  #ifndef __ARM_PCS_VFP
  # error Not hardfloat
  #endif
  '''
  if cc.compiles(is_hardfloat_src, name: 'hardfloat ABI')
    host_abi = 'armhf'
  endif
endif

have_ptrauth_src = '''
#ifdef __clang__
# if __has_feature (ptrauth_calls)
#  define HAVE_PTRAUTH 1
# endif
#endif

#ifndef HAVE_PTRAUTH
# error Pointer authentication not supported
#endif
'''
have_ptrauth = cc.compiles(have_ptrauth_src, name: 'pointer authentication')

if host_os_family == 'darwin' and host_arch == 'arm64' and have_ptrauth
  host_abi = 'arm64e'
endif

if cc.sizeof('void *') == 8
  host_cpu_mode = '64'
else
  host_cpu_mode = '32'
endif

if host_os_family == 'windows'
  host_executable_format = 'pe'
elif host_os_family == 'darwin'
  host_executable_format = 'macho'
elif host_os_family in ['linux', 'freebsd', 'qnx']
  host_executable_format = 'elf'
elif host_os_family == 'none'
  host_executable_format = 'none'
else
  error('Unsupported OS family:', host_os_family)
endif

extra_deps = []
extra_requires_private = []
extra_cflags = []
extra_libs_private = []
extra_gir_args_private = []

cdata = configuration_data()

cdata.set_quoted('FRIDA_VERSION', frida_version)

static = get_option('default_library') == 'static'
if static
  extra_cflags += '-DGUM_STATIC'
  cdata.set('GUM_STATIC', 1)
endif

diet = get_option('diet')
if diet and not optimize_for_prod
  error('Diet mode requires -Db_ndebug=true')
endif

gumpp_opt = get_option('gumpp')
gumjs_opt = get_option('gumjs')
inspector_opt = get_option('inspector')
have_gumpp = not gumpp_opt.disabled()
have_gumjs = not gumjs_opt.disabled()
if inspector_opt.auto()
  have_inspector = have_gumjs and host_os_family != 'none'
else
  have_inspector = inspector_opt.enabled()
  if have_inspector and host_os_family == 'none'
    error('Inspector not available for OS-less builds')
  endif
endif

tests_opt = get_option('tests')
if tests_opt.auto()
  build_tests = not meson.is_subproject() and meson.can_run_host_binaries()
else
  build_tests = not tests_opt.disabled()
endif

allocator = get_option('allocator')
if allocator == 'auto'
  allocator = ((host_os_family == 'none') or diet) ? 'system' : 'internal'
endif
if host_os_family == 'none' and allocator != 'system'
  error('Internal allocator is not supported for os == none')
endif
if allocator == 'system'
  cdata.set('GUM_USE_SYSTEM_ALLOC', 1)
endif

have_jailbreak = get_option('jailbreak').allowed() and host_os in ['macos', 'ios', 'tvos']
if have_jailbreak
  cdata.set('HAVE_JAILBREAK', 1)
endif

if host_os_family != 'none'
  cdata.set('HAVE_' + host_os_family.to_upper(), 1)
  if host_os != host_os_family
    cdata.set('HAVE_' + host_os.to_upper(), 1)
  endif
endif

cpu_defines = [
  ['x86', 'HAVE_I386'],
  ['x86_64', 'HAVE_I386'],
  ['arm', 'HAVE_ARM'],
  ['arm64', 'HAVE_ARM64'],
  ['mips', 'HAVE_MIPS'],
  ['mips64', 'HAVE_MIPS'],
]
foreach d : cpu_defines
  if d.get(0) == host_arch
    cdata.set(d.get(1), 1)
  endif
endforeach

if host_executable_format == 'elf'
  cdata.set('HAVE_ELF', 1)
endif

# Which convention the code we instrument follows. It is our own everywhere except on a
# bare-metal build aimed at a kernel that does not share it, and nothing about the compiler
# says so, the machine being what knows.
if meson.get_external_property('target_abi', '') == 'microsoft'
  cdata.set('GUM_TARGET_ABI_WINDOWS', 1)
endif

if have_ptrauth
  cdata.set('HAVE_PTRAUTH', 1)
endif

headers = [
  'elf.h',
  'link.h',
  'stdint.h',
  'sys/auxv.h',
  'sys/elf.h',
  'asm/ptrace.h',
  'sys/user.h',
]
foreach h : headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach
if cc.compiles('#include <asm/prctl.h>', name: 'asm/prctl.h is available')
  cdata.set('HAVE_ASM_PRCTL_H', 1)
endif

functions = [
  ['madvise', []],
  ['posix_madvise', []],
  ['posix_spawnattr_init', ['spawn.h']]
]
foreach f : functions
  name = f[0]
  headers_needed = f[1]

  prefix_lines = []
  foreach header : headers_needed
    prefix_lines += [f'#include <@header@>']
  endforeach

  if cc.has_function(name, prefix: '\n'.join(prefix_lines))
    cdata.set('HAVE_' + name.to_upper(), 1)
  endif
endforeach

types = [
  'long double',
  'long long int',
  'unsigned long long int'
]
foreach t : types
  if cc.has_type(t)
    cdata.set('HAVE_' + t.underscorify().to_upper(), 1)
  endif
endforeach

if host_os == 'linux'
  glibc_src = '''
#include <features.h>

#if defined (__GLIBC__) && !defined (__UCLIBC__)
#else
# error Not glibc
#endif
'''
  uclibc_src = '''
#include <features.h>

#if !defined (__UCLIBC__)
# error Not uClibc
#endif
'''
  if cc.compiles(glibc_src, name: 'compiling for glibc')
    cdata.set('HAVE_GLIBC', 1)
  elif cc.compiles(uclibc_src, name: 'compiling for uClibc')
    cdata.set('HAVE_UCLIBC', 1)
  else
    cdata.set('HAVE_MUSL', 1)
  endif
endif

if cc.has_member('struct mallinfo', 'arena', prefix: '#include <malloc.h>')
  cdata.set('HAVE_LIBC_MALLINFO', 1)
endif

if get_option('b_sanitize') == 'address'
  cdata.set('HAVE_ASAN', 1)
endif

if cc.compiles('''#pragma pack (push, 1)
                  struct _Foo
                  {
                    short a;
                    int b;
                  };
                  #pragma pack (pop)
                  ''' + '\n',
               args: ['-Werror'],
               name: 'toolchain supports the pack pragma')
  cdata.set('HAVE_PACK_PRAGMA', 1)
endif

if cc.compiles('''int
                  acquire (int * lock)
                  {
                    return __sync_lock_test_and_set (lock, 1);
                  }
                  ''' + '\n',
               args: ['-Werror'],
               name: 'toolchain supports __sync_lock_test_and_set()')
  cdata.set('HAVE_SYNC_LOCK', 1)
endif

if cc.compiles('''void
                  invalidate (void * start,
                              void * end)
                  {
                    __builtin___clear_cache (start, end);
                  }
                  ''' + '\n',
               args: ['-Werror'],
               name: 'toolchain supports __builtin___clear_cache()')
  cdata.set('HAVE_CLEAR_CACHE', 1)
endif

if cc.compiles('''int
                  count_zeros (unsigned long long x)
                  {
                    return __builtin_clzll (x) + __builtin_ctzll (x);
                  }
                  ''' + '\n',
               args: ['-Werror'],
               name: 'toolchain supports __builtin_c[lt]z*()')
  cdata.set('HAVE_CLTZ', 1)
endif

if cc.compiles('''int
                  num_one_bits (unsigned int x)
                  {
                    return __builtin_popcount (x);
                  }
                  ''' + '\n',
               args: ['-Werror'],
               name: 'toolchain supports __builtin_popcount()')
  cdata.set('HAVE_POPCOUNT', 1)
endif

threads_dep = dependency('threads')

if allocator == 'internal' or have_gumjs
  ffi_options = [
    'default_library=static',
    'exe_static_tramp=false',
    'tests=false',
  ]
  ffi_dep = dependency('libffi', default_options: ffi_options)
else
  ffi_dep = disabler()
endif

glib_version_req = '>=2.72'
glib_options = [
  'default_library=static',
  'diet=' + diet.to_string(),
  'printf=' + allocator,
  'cocoa=disabled',
  'selinux=disabled',
  'xattr=false',
  'libmount=disabled',
  'tests=false',
  'nls=disabled',
]
if optimize_for_prod
  glib_options += [
    'glib_debug=disabled',
    'glib_assert=false',
    'glib_checks=false',
  ]
endif
glib_dep = dependency('glib-2.0', version: glib_version_req, default_options: glib_options)
gobject_dep = dependency('gobject-2.0')
gio_os_package_name = (host_os_family == 'windows') ? 'gio-windows-2.0' : 'gio-unix-2.0'
gio_dep = dependency('gio-2.0')
gio_os_package_dep = dependency(gio_os_package_name)

frida_glib_probe = '''
#include <glib.h>
void * _frida_detect_glib_fork (void) { return (void *) g_thread_set_callbacks; }
'''

glib_dep_native = dependency('glib-2.0', version: glib_version_req, native: true, default_options: glib_options)
native_cc = meson.get_compiler('c', native: true)
if glib_dep_native.type_name() == 'internal' or native_cc.compiles(frida_glib_probe, dependencies: [glib_dep_native], name: 'Frida GLib fork (native)')
  add_project_arguments('-DHAVE_FRIDA_GLIB=1', language: languages, native: true)
endif
gio_dep_native = dependency('gio-2.0', native: true)

capstone_options = [
  'default_library=static',
  'profile=' + (diet ? 'tiny' : 'full'),
  'archs=all',
  'use_arch_registration=true',
  'x86_att_disable=true',
  'cli=disabled',
]
capstone_dep = dependency('capstone', version: '>=5.0.0', default_options: capstone_options)
if capstone_dep.type_name() != 'internal' \
    and not cc.has_header_symbol('capstone.h', 'cs_arch_register_x86',
      dependencies: [capstone_dep])
  add_project_arguments(
    '-Dcs_arch_register_x86()=',
    '-Dcs_arch_register_arm()=',
    '-Dcs_arch_register_arm64()=',
    '-Dcs_arch_register_mips()=',
    language: languages,
  )
endif

lzma_dep = dependency('liblzma', required: build_tests, default_options: ['default_library=static'])
if lzma_dep.found()
  cdata.set('HAVE_LZMA', 1)
  extra_requires_private += 'liblzma'
endif

libtcc_dep = dependency('libtcc',
  required: (host_abi in ['arm', 'arm64'] and host_machine.endian() == 'little')
            or (host_abi not in ['arm', 'arm64'] and host_arch not in ['mips', 'mips64']),
  default_options: ['default_library=static'],
)
cdata.set('HAVE_TINYCC', libtcc_dep.found())

json_glib_dep = dependency('json-glib-1.0', default_options: [
  'default_library=static',
  'introspection=disabled',
  'gtk_doc=disabled',
  'tests=false',
])

if glib_dep.type_name() == 'internal'
  have_shared_glib = not static
else
  glib_flavor_src = '''
#include <glib.h>

#ifdef GLIB_STATIC_COMPILATION
# error GLib is static
#endif
'''
  have_shared_glib = cc.compiles(glib_flavor_src, name: 'compiling with shared GLib', dependencies: [glib_dep])
endif
if not static and have_shared_glib and not meson.is_cross_build()
  gir_scanner = find_program('g-ir-scanner', required: false)
  gir_dep = dependency('gobject-introspection-1.0', required: gir_scanner.found())
else
  gir_scanner = disabler()
  gir_dep = disabler()
endif

if get_option('devkits').length() != 0
  if not static
    error('Devkits can only be generated from static libraries')
  endif
  mkdevkit = [python, releng / 'mkdevkit.py']
  uninstalled_dir = meson.global_build_root() / 'meson-uninstalled'
  devkit_options = [
    '--dep-symbol-scope', get_option('devkit_symbol_scope'),
    '--cc', '>>>', cc.cmd_array(), '<<<',
    '--c-args', '>>>', get_option('c_args'), '<<<',
    '--pkg-config', '>>>', find_program('pkg-config'), '<<<',
    '--pkg-config-path', '>>>', uninstalled_dir, get_option('pkg_config_path'), '<<<',
  ]
  if cc.get_argument_syntax() == 'msvc'
    static_lib_prefix = ''
    static_lib_suffix = '.lib'
    devkit_options += ['--lib', '>>>', find_program('lib'), '<<<']
  else
    static_lib_prefix = 'lib'
    static_lib_suffix = '.a'
    foreach tool : ['ar', 'nm', 'objcopy']
      p = find_program(tool, required: false)
      if p.found()
        devkit_options += ['--' + tool, p]
      endif
    endforeach
  endif
  if host_os_family == 'darwin'
    devkit_options += ['--libtool', find_program('libtool')]
  endif
endif

if glib_dep.type_name() == 'internal' or cc.compiles(frida_glib_probe, dependencies: [glib_dep], name: 'Frida GLib fork')
  cdata.set('HAVE_FRIDA_GLIB', 1)
endif

if ffi_dep.found() and (ffi_dep.type_name() == 'internal' or cc.has_function('ffi_set_mem_callbacks', dependencies: [ffi_dep]))
  cdata.set('HAVE_FRIDA_LIBFFI', 1)
endif

pthread_functions = [
  'pthread_attr_getstack',
  'pthread_setname_np',
]
foreach f : pthread_functions
  if cc.has_function(f,
      args: ['-D_GNU_SOURCE'],
      dependencies: [threads_dep],
      prefix: '#include <pthread.h>')
    cdata.set('HAVE_' + f.to_upper(), 1)
  endif
endforeach

if host_os_family == 'windows'
  extra_deps += cc.find_library('psapi')
endif

if host_os == 'linux'
  if host_arch == 'arm' and cc.has_member('mcontext_t', 'gregs', prefix: '#include <ucontext.h>')
    cdata.set('HAVE_LEGACY_MCONTEXT', 1)
  endif

  extra_libs_private += ['-ldl']
  extra_gir_args_private += ['--extra-library=dl']
endif

if host_os == 'android'
  extra_libs_private += ['-llog']
endif

if host_os_family in ['linux', 'freebsd', 'qnx']
  libunwind_dep = dependency('libunwind', default_options: [
    'default_library=static',
    'generic_library=disabled',
    'coredump_library=disabled',
    'ptrace_library=disabled',
    'setjmp_library=disabled',
    'msabi_support=false',
    'minidebuginfo=enabled',
    'zlibdebuginfo=enabled',
  ])
  cdata.set('HAVE_LIBUNWIND', 1)
  extra_requires_private += ['libunwind']

  libdwarf_dep = dependency('libdwarf', required: false, default_options: ['default_library=static'])
  if libdwarf_dep.found() and libdwarf_dep.type_name() != 'internal' \
      and not cc.has_header('dwarf.h', dependencies: libdwarf_dep)
    libdwarf_dep = disabler()
  endif
  if not libdwarf_dep.found()
    found = false
    if not meson.is_cross_build()
      foreach idir : ['/usr/include/libdwarf', '/usr/local/include/libdwarf']
        inc_arg = '-I' + idir
        if not found and cc.has_header_symbol('libdwarf.h', 'dwarf_elf_init_b', args: inc_arg)
          libdwarf_dep = declare_dependency(compile_args: [inc_arg])
          extra_libs_private += '-ldwarf'
          extra_gir_args_private += '--extra-library=dwarf'
          found = true
        endif
      endforeach
    endif
    if not found
      libdwarf_dep = dependency('libdwarf', required: true, default_options: ['default_library=static'])
    endif
  endif

  extra_deps += [libunwind_dep, libdwarf_dep]
else
  libunwind_dep = disabler()
  libdwarf_dep = disabler()
endif

if host_os == 'android'
  minizip_dep = dependency('minizip', required: false, default_options: [
    'default_library=static',
    'zlib=enabled',
    'lzma=disabled',
  ])
  if minizip_dep.found()
    cdata.set('HAVE_MINIZIP', 1)
    extra_deps += [minizip_dep]
    extra_requires_private += ['minizip']
  endif
endif

openssl_options = [
  'default_library=static',
  'cli=disabled',
]
if host_os_family == 'windows' and cc.get_argument_syntax() != 'msvc'
  openssl_options += 'asm=disabled'
endif
dependency('openssl',
  required: false,
  modules: cc.get_argument_syntax() == 'msvc' ? ['OpenSSL::SSL'] : [],
  default_options: openssl_options,
)

tls_provider_dep = dependency('gioopenssl', required: false, default_options: [
  'default_library=static',
  'gnutls=disabled',
  'openssl=enabled',
  'libproxy=disabled',
  'gnome_proxy=disabled',
  'tests=false',
])
if tls_provider_dep.found()
  cdata.set('HAVE_GIOOPENSSL', 1)
endif

if have_gumpp
  cdata.set('HAVE_GUMPP', 1)
endif

gumjs_extra_requires = []
if have_inspector
  libsoup_dep = dependency('libsoup-3.0',
    required: have_inspector,
    default_options: [
      'default_library=static',
      'gssapi=disabled',
      'ntlm=disabled',
      'brotli=disabled',
      'tls_check=false',
      'introspection=disabled',
      'vapi=disabled',
      'docs=disabled',
      'examples=disabled',
      'tests=false',
      'sysprof=disabled',
    ],
  )
endif

if have_gumjs
  cdata.set('HAVE_GUMJS', 1)

  quickjs_options = [
    'default_library=static',
    'libc=false',
    'bignum=true',
    'atomics=disabled',
    'stack_check=' + ((host_os_family == 'none') ? 'enabled' : 'disabled'),
    'stack_mode=' + ((host_os_family == 'none') ? 'optimize' : 'default'),
  ]

  if host_os_family == 'none'
    # A floor, not a measurement of the target in front of us: the runtime gets
    # whatever stack its host hands a kernel thread, and the parser overshoots the
    # limit before rechecking. 14 KiB overflows on arm64 Linux, 12 KiB does not.
    cdata.set('HAVE_TINY_STACK', 1)
    cdata.set('GUM_QUICK_TINY_STACK_SIZE', 12 * 1024)
  endif

  quickjs_dep = dependency('quickjs', required: get_option('quickjs'), default_options: quickjs_options)
  cdata.set('HAVE_QUICKJS', quickjs_dep.found())
  if quickjs_dep.found()
    gumjs_extra_requires += 'quickjs'
    quickjs_dep_native = dependency('quickjs', native: true, default_options: quickjs_options)
  endif

  v8_dep = dependency('v8-10.0',
    version: '>=10.6.122',
    required: get_option('v8'),
    default_options: [
      'default_library=static',
      'debug=false',
      'embedder_string=-frida',
      'snapshot_compression=disabled',
      'pointer_compression=disabled',
      'shared_ro_heap=disabled',
      'cppgc_caged_heap=disabled',
    ],
  )
  cdata.set('HAVE_V8', v8_dep.found())
  if v8_dep.found()
    gumjs_extra_requires += 'v8-10.0'
  endif

  if not (quickjs_dep.found() or v8_dep.found())
    error('Cannot build JavaScript bindings without any runtimes enabled')
  endif

  gumjs_extra_requires += [gio_os_package_name]

  if host_arch == 'arm' and cc.has_function('__aeabi_memmove')
    cdata.set('HAVE_AEABI_MEMORY_BUILTINS', 1)
  endif

  sqlite_dep = dependency('sqlite3', required: get_option('database'), default_options: ['default_library=static'])
  cdata.set('HAVE_SQLITE', sqlite_dep.found())
else
  v8_dep = disabler()
endif

configure_file(
  output: 'config.h',
  configuration: cdata)

add_project_arguments(
  (cc.get_argument_syntax() == 'msvc') ? '/FI' : '-include', meson.current_build_dir() / 'config.h',
  '-DG_LOG_DOMAIN="Frida"',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_56',
  '-DG_DISABLE_DEPRECATED',
  language: languages)

if host_os_family == 'windows'
  add_project_arguments(
    '-DPSAPI_VERSION=1',
    language: languages)
endif

if cc.get_argument_syntax() == 'msvc'
  add_project_arguments(
    '/wd4116',
    '/wd4244',
    '/wd5051',
    language: languages)
endif

if host_os_family == 'linux'
  add_project_arguments('-D_GNU_SOURCE=1', language: languages)
endif

if host_os_family == 'qnx'
  add_project_arguments('-D_QNX_SOURCE=1', language: languages)
endif

gum_incdirs = [
  include_directories('.'),
  include_directories('gum'),
  include_directories('gum' / 'arch-x86'),
  include_directories('gum' / 'arch-arm'),
  include_directories('gum' / 'arch-arm64'),
  include_directories('gum' / 'arch-mips'),
  include_directories('libs'),
  include_directories('libs' / 'gum' / 'heap'),
  include_directories('libs' / 'gum' / 'prof'),
]

if host_os_family == 'windows'
  gum_incdirs += [
    include_directories('gum' / 'backend-windows' / 'include'),
    include_directories('gum' / 'backend-dbghelp' / 'include'),
    include_directories('ext' / 'dbghelp'),
  ]
endif

if host_os_family == 'darwin'
  gum_incdirs += include_directories('gum' / 'backend-darwin' / 'include')
endif

if host_os_family == 'linux'
  gum_incdirs += include_directories('gum' / 'backend-linux' / 'include')
endif

if host_os_family == 'freebsd'
  gum_incdirs += include_directories('gum' / 'backend-freebsd' / 'include')
endif

if host_os_family == 'qnx'
  gum_incdirs += include_directories('gum' / 'backend-qnx' / 'include')
endif

if host_os_family == 'none'
  gum_incdirs += include_directories('gum' / 'backend-barebone' / 'include')
endif

if host_executable_format == 'elf'
  gum_incdirs += include_directories('gum' / 'backend-elf')
endif

if libunwind_dep.found()
  gum_incdirs += include_directories('gum' / 'backend-libunwind' / 'include')
endif

frida_libdir_name = 'frida-' + api_version
install_header_basedir = frida_libdir_name
install_header_subdir = install_header_basedir + '/gum'

subdir('ext')
subdir('gum')
subdir('libs')
subdir('bindings')
subdir('vapi')
if host_os_family != 'none'
  subdir('tools')
endif

if build_tests
  subdir('tests')
endif
