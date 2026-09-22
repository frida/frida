# Comment on the first line
project('trivial test',
  # Comment inside a function call + array for language list
  ['c'], default_options: ['buildtype=debug'],
  meson_version : '>=0.52.0')
#this is a comment
sources = 'trivial.c'

cc = meson.get_compiler('c')
if cc.get_id() == 'intel'
  # Error out if the -std=xxx option is incorrect
  add_project_arguments('-diag-error', '10159', language : 'c')
elif cc.get_id() == 'intel-cl'
  add_project_arguments('/Qdiag-error:10159', language : 'c')
endif

exe = executable('trivialprog', sources : sources)
assert(exe.name() == 'trivialprog')
test('runtest', exe) # This is a comment

has_not_changed = false
if is_disabler(exe)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Executable has changed.')

assert(not is_disabler(exe), 'Executable is a disabler.')
