subdirgen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

generated = subdirgen.process('defs.proto')
e = executable('subdir-prog', 'main.cpp', generated,
 dependencies : dep)
test('subdir-prototest', e)
