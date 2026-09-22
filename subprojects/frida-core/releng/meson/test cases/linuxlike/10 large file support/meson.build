project('trivial test', 'c')

if host_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST _FILE_OFFSET_BITS not yet supported on Cygwin.')
endif

cc = meson.get_compiler('c')

size = cc.sizeof('off_t')
assert(size == 8, 'off_t size is @0@ bytes instead of 8'.format(size))

code = '''#if !defined(_FILE_OFFSET_BITS) || (_FILE_OFFSET_BITS != 64)
#error "Large-file support was not enabled"
#endif'''

assert(cc.compiles(code, name : 'checking for LFS'), 'Large file support was not enabled')
