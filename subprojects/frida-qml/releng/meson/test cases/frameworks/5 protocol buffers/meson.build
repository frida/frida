project('protocol buffer test', 'cpp', default_options: ['cpp_std=c++17'])

protoc = find_program('protoc', required : false)
dep = dependency('protobuf', required : false)

if not protoc.found() or not dep.found()
  error('MESON_SKIP_TEST: protoc tool and/or protobuf pkg-config dependency not found')
endif


gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

generated = gen.process('defs.proto')
e = executable('prog', 'main.cpp', generated,
 dependencies : dep)
test('prototest', e)

subdir('asubdir')
subdir('withpath')
subdir('sidedir')
