project('external dependency with static', 'c')

# Zlib is probably on all dev machines.

dep = dependency('zlib')
statlib = static_library('statlib', 'lib.c', dependencies : dep)
exe = executable('prog', 'prog.c', link_with : statlib)


test('zlibtest', exe)
