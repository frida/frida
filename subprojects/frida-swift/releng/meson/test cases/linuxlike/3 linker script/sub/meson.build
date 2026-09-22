mapfile = 'foo.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

l = shared_library('foo', '../bob.c', link_args : vflag, link_depends : mapfile)
e = executable('prog-foo', '../prog.c', link_with : l)
test('core', e)
