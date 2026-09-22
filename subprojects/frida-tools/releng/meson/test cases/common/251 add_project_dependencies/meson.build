project('zlib system dependency', 'c')

cc = meson.get_compiler('c')

m = cc.find_library('m', required: false)
add_project_dependencies(m, language: ['c'])

z = dependency('zlib', method: 'system', required: false)
if not z.found()
  error('MESON_SKIP_TEST zlib not present')
endif

z_c_args = z.partial_dependency(compile_args: true, includes: true)
add_project_dependencies(z_c_args, language: 'c', native: false)

global_dep = declare_dependency(include_directories: include_directories('inc'),
                                compile_args: '-DDEFINED')
add_project_dependencies(global_dep, language: 'c', native: false)

lib = static_library('rary', 'lib.c')
exe = executable('prog', 'main.c', link_with: lib, dependencies: z)
test('test', exe)
