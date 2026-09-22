project('llvmtest', ['c', 'cpp'], default_options : ['c_std=c99'])

method = get_option('method')
static = get_option('link-static')

d = dependency('llvm', required : false, static : static)
if not d.found()
  error('MESON_SKIP_TEST llvm not found.')
endif

modules_to_find = [
  'bitwriter', 'asmprinter', 'executionengine', 'mcjit', 'target',
  'nativecodegen', 'amdgpu', 'engine'
]

if method == 'combination'
  if not d.version().version_compare(static ? '>0.1' : '>=7.0')
    error('MESON_SKIP_TEST: llvm version is too low')
  endif
  llvm_ct_dep = dependency(
    'llvm',
    modules : modules_to_find,
    required : false,
    static : static,
    method : 'config-tool',
  )

  if static and d.version().startswith('16.0') and d.version()[5].to_int() <= 6
    message('Skipping modules with cmake due to an LLVM bug, see https://github.com/mesonbuild/meson/issues/11642')
    llvm_cm_dep = dependency(
      'llvm',
      required : false,
      static : static,
      method : 'cmake',
    )
  else
    llvm_cm_dep = dependency(
      'llvm',
      modules : modules_to_find,
      required : false,
      static : static,
      method : 'cmake',
    )
  endif

  assert(llvm_ct_dep.found() and llvm_cm_dep.found(), 'config-tool and cmake both need to be found')
  cm_version_major = llvm_cm_dep.version().split('.')[0].to_int()
  cm_version_minor = llvm_cm_dep.version().split('.')[1].to_int()
  ct_version_major = llvm_ct_dep.version().split('.')[0].to_int()
  ct_version_minor = llvm_ct_dep.version().split('.')[1].to_int()
  assert(cm_version_major == ct_version_major, 'config-tool and cmake returns different major versions')
  assert(cm_version_minor == ct_version_minor, 'config-tool and cmake returns different minor versions')
else
  if not static and method == 'cmake'
    d = dependency('llvm', version : '>=7.0', required : false, static : static)
    if not d.found()
      error('MESON_SKIP_TEST llvm version is too low for cmake dynamic link.')
    endif
  endif

  d = dependency('llvm', modules : 'not-found', required : false, static : static, method : method)
  assert(d.found() == false, 'not-found llvm module found')

  d = dependency('llvm', version : '<0.1', required : false, static : static, method : method)
  assert(d.found() == false, 'ancient llvm module found')

  d = dependency('llvm', optional_modules : 'not-found', required : false, static : static, method : method)
  assert(d.found() == true, 'optional module stopped llvm from being found.')

  # Check we can apply a version constraint
  d = dependency('llvm', version : ['< 500', '>=@0@'.format(d.version())], required: false, static : static, method : method)
  assert(d.found() == true, 'Cannot set version constraints')

  # Check if we have to get pseudo components
  d = dependency('llvm', modules: ['all-targets','native','engine'], required: false, static : static, method : method)
  assert(d.found() == true, 'Cannot find pseudo components')

  dep_tinfo = dependency('tinfo', required : false)
  if not dep_tinfo.found()
    cpp = meson.get_compiler('cpp')
    dep_tinfo = cpp.find_library('tinfo', required: false)
  endif

  if static and method == 'cmake' and d.version().startswith('16.0')
    message('Skipping modules with cmake due to an LLVM bug, see https://github.com/mesonbuild/meson/issues/11642')
    llvm_dep = dependency(
      'llvm',
      required : false,
      static : static,
      method : method,
    )
  else
    llvm_dep = dependency(
      'llvm',
      modules : modules_to_find,
      required : false,
      static : static,
      method : method,
    )
  endif

  if not llvm_dep.found()
    error('MESON_SKIP_TEST required llvm modules not found.')
  endif

  executable(
      'sum',
      'sum.c',
      dependencies : [
        llvm_dep, dep_tinfo,
        # zlib will be statically linked on windows
        dependency('zlib', required : host_machine.system() != 'windows'),
        meson.get_compiler('c').find_library('dl', required : false),
      ]
    )
endif
