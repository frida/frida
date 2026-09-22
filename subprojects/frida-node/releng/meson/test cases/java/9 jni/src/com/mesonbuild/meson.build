configured = configure_file(
  input: files('Configured.java.in'),
  output: 'Configured.java',
  configuration: configuration_data({'fingerprint': '0xdeadbeef'})
)

sources += configured

native_headers = javamod.native_headers(
  sources, package: 'com.mesonbuild', classes: ['JniTest'])
native_header_includes = include_directories('.')
