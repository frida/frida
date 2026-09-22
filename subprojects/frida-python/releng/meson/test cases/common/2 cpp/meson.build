project('c++ test', 'cpp', version: files('VERSIONFILE'))

cpp = meson.get_compiler('cpp')
if cpp.get_id() == 'intel'
  # Error out if the -std=xxx option is incorrect
  add_project_arguments('-diag-error', '10159', language : 'cpp')
elif cpp.get_id() == 'intel-cl'
  add_project_arguments('/Qdiag-error:10159', language : 'cpp')
endif

exe = executable('trivialprog', 'trivial.cc', extra_files : 'something.txt')
test('runtest', exe)

has_not_changed = false
if is_disabler(exe)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Executable has changed.')

assert(not is_disabler(exe), 'Executable is a disabler.')

exe = executable('trivialprog', 'trivial.cc', extra_files : disabler())

assert(is_disabler(exe), 'Executable is not a disabler.')

if exe.found()
  exe_disabled = false
else
  exe_disabled = true
endif

assert(exe_disabled, 'Executable was not disabled.')

if cpp.get_id() == 'msvc'
  exe = executable('cppprog', 'cpp.C', cpp_args : '/TP')
else
  exe = executable('cppprog', 'cpp.C')
endif
test('cpptest', exe)
