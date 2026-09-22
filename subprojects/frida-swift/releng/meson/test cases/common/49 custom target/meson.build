project('custom target')

python = find_program('python3', required : false)
if not python.found()
  python = find_program('python')
endif

# Note that this will not add a dependency to the compiler executable.
# Code will not be rebuilt if it changes.
comp = '@0@/@1@'.format(meson.current_source_dir(), 'my_compiler.py')
# Test that files() in command: works. The compiler just discards it.
useless = files('test.json')

mytarget = custom_target('bindat',
output : 'data.dat',
input : 'data_source.txt',
command : [python, comp, '--input=@INPUT@', '--output=@OUTPUT@', useless],
env: {'MY_COMPILER_ENV': 'value'},
install : true,
install_dir : 'subdir'
)

has_not_changed = false
if is_disabler(mytarget)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Custom target has changed.')

assert(not is_disabler(mytarget), 'Custom target is a disabler.')

mytarget_disabler = custom_target('bindat',
output : 'data.dat',
input : 'data_source.txt',
command : [disabler(), comp, '--input=@INPUT@', '--output=@OUTPUT@', useless],
install : true,
install_dir : 'subdir'
)

if mytarget_disabler.found()
  mytarget_disabled = false
else
  mytarget_disabled = true
endif

assert(mytarget_disabled, 'Disabled custom target should not be found.')

mytarget_ci = custom_target('bindat_ci',
output : 'data_ci.dat',
input : 'data_source.txt',
command : [python, comp, '--input=@INPUT@', '--output=@OUTPUT@', mytarget.to_list()],
)

mytarget_disabler = custom_target('bindat',
output : 'data.dat',
input : disabler(),
command : [python, comp, '--input=@INPUT@', '--output=@OUTPUT@', useless],
install : true,
install_dir : 'subdir'
)

assert(is_disabler(mytarget_disabler), 'Disabled custom target is not a disabler.')

if mytarget_disabler.found()
  mytarget_disabled = false
else
  mytarget_disabled = true
endif

assert(mytarget_disabled, 'Disabled custom target should not be found.')

subdir('depfile')
