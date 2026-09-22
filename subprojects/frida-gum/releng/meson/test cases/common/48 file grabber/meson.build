project('grabber', 'c')

# What this script does is NOT reliable. Simply adding a file in this directory
# will NOT make it automatically appear in the build. You have to manually
# re-invoke Meson (not just Ninja) for that to happen. The simplest way
# is to touch meson-private/coredata.dat.

# This is not the recommended way to do things, but if the tradeoffs are
# acceptable to you, then we're certainly not going to stop you. Just don't
# file bugs when it fails. :)

if build_machine.system() == 'windows'
  c = run_command('grabber.bat', check: false)
  grabber = find_program('grabber2.bat')
else
  c = run_command('grabber.sh', check: false)
  grabber = find_program('grabber.sh')
endif


# First test running command explicitly.
if c.returncode() != 0
  error('Executing script failed.')
endif

newline = '''
'''

sources = c.stdout().strip().split(newline)

e = executable('prog', sources)
test('grabtest', e)

# Then test using program with find_program
subdir('subdir')
