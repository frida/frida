# Try to invoke linker constant string deduplication,
# to ensure we are not clobbering shared strings.
# Name everything possible just as "foo".
foolib = shared_library('foo', 'foo.c',
  install_dir : 'foo',
  install : true)
