# Test that using files generated with configure_file as sources works.
# We do this inside a subdir so that the path isn't accidentally correct
# because there is no structure in the build dir.
genlib = configure_file(input : '../lib.c',
  output : 'genlib.c',
  copy: true)
shared_library('genlib', genlib,
  install : false)
