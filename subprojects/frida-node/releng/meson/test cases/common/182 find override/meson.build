project('find program override', 'c')

gencodegen = find_program('gencodegen', required : false)
six_prog = find_program('six_meson_exe', required : false)

assert(not gencodegen.found(), 'gencodegen is an internal program, should not be found')
assert(not six_prog.found(), 'six_meson_exe is an internal program, should not be found')

# Test the check-if-found-else-override workflow
if not gencodegen.found()
  subdir('subdir')
endif

subdir('otherdir')

tool = find_program('sometool')
assert(tool.found())
assert(tool.full_path() != '')
assert(tool.full_path() == tool.path())

# six_meson_exe is an overridden project executable
six_prog = find_program('six_meson_exe')
assert(six_prog.found())
assert(six_prog.full_path() != '')
assert(six_prog.full_path() == six_prog.path())

# We have prog-version.py in current directory, but it's version 1.0.
# This needs to use fallback for "prog-version" name which will be version 2.0.
prog = find_program('prog-version.py', 'prog-version', version: '>= 2.0')
assert(prog.found())
assert(prog.version() == '2.0')
