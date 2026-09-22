project('file object', 'c')

prog0 = files('prog.c')
lib0 = files('lib.c')
test('fobj', executable('fobj', prog0, lib0))

subdir('subdir1')
subdir('subdir2')

# Use fs.as_posix() because / operator replaces \ with / in the path, but
# full_path() method is not doing that. This is a pretty inconsistent across all
# Meson APIs.
fs = import('fs')
assert(fs.as_posix(prog0[0].full_path()) == fs.as_posix(meson.current_source_dir() / 'prog.c'))
