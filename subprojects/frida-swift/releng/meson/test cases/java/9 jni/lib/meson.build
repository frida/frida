sources = [
  files(
    'native.c',
    'com_mesonbuild_JniTest.c',
  ),
  native_headers
]

jnijava = shared_module(
  'jnijava',
  sources,
  dependencies : [jni_dep],
  include_directories : [native_header_includes]
)

jnijava_dep = declare_dependency(
  link_with : jnijava
)
