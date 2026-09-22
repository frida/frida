project('structured input', 'rust')

foo_mod_rs = configure_file(
  input : 'src/foo.rs.in',
  output : 'mod.rs',
  configuration : {'message' : 'Hello, World!'},
)

conf_file = executable(
  'main_conf_file',
  structured_sources(
    'src/main.rs',
    {'foo' : [foo_mod_rs]},
  ),
)

ct = custom_target(
  'foo.rs',
  output : 'foo.rs',
  command : ['gen.py', '@OUTPUT@'],
)

target = executable(
  'main_custom_target',
  structured_sources(
    ['src/main.rs', ct],
  ),
)

# Should not be copied
executable(
  'no_copy_target',
  structured_sources(
    ['src2/main-unique.rs'],
    {'foo': 'src2/foo/mod.rs'},
  ),
)

test('no-copy', find_program('no_copy_test.py'), args : meson.current_build_dir())

subdir('src2')

executable('copy-no-gen', srcs2)

m_src = configure_file(
  input : 'main-gen-copy.rs',
  output : 'main-gen-copy.rs',
  configuration : {'dir' : meson.current_build_dir().replace('\\', '/')},
)

m_src2 = configure_file(
  input : 'priv.rs',
  output : 'include.rs',
  copy : true
)

executable('gen-no-copy', structured_sources([m_src, m_src2]))

