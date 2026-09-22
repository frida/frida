project('system_include_dir', 'cpp',
    version : '0.1',
    default_options : 'werror=true',
)

compiler_id = meson.get_compiler('cpp').get_id()
if not ['gcc', 'clang', 'clang-cl'].contains(compiler_id)
  error('MESON_SKIP_TEST: compiler @0@ either doesn\'t support is_system includes or needs to have support for this test added'.format(compiler_id))
endif

lib_include_directories = include_directories('lib', is_system: true)
add_project_arguments('-Wsign-conversion', language: 'cpp')
executable('system_include_dir_test', sources: 'main.cpp', include_directories: lib_include_directories)
