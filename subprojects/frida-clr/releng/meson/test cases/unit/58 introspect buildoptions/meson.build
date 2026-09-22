project('introspect buildargs', ['c'], default_options: ['c_std=c99', 'cpp_std=c++14', 'buildtype=release'])

subA = subproject('projectA')

target_name = 'MAIN'
target_src = ['main.c']

foo = false

executable(target_name, target_src, build_by_default : foo)

r = run_command(find_program('c_compiler.py'), check: false)
if r.returncode() != 0
  error('FAILED')
endif

add_languages(r.stdout().strip(), required: true)
add_languages('afgggergearvearghergervergreaergaergasv', required: false)
