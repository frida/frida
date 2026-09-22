project('myexe', 'c')

foo = executable('foo', 'foo.c')
meson.override_find_program('bar', foo)
bar = find_program('bar')
run_command(bar, check: true)
