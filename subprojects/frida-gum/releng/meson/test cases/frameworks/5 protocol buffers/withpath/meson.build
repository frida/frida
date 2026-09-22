# Testing protobuf files that are deeply hierarchical
# and must preserve their path segments in output files
# because protoc will always put it in there.

generated = gen.process('com/mesonbuild/simple.proto',
  'com/mesonbuild/subsite/complex.proto',
  preserve_path_from : meson.current_source_dir(),
  )

e = executable('pathprog', 'pathprog.cpp', generated,
  override_options : ['unity=off'],
  dependencies : dep)
test('pathprog', e)
