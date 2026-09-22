project('trivial test', 'c')

python = find_program('python3')

exe = executable('trivialprog', 'trivial.c')
runt = run_target('invalid', command: [python, '--version'])
meson.add_install_script(exe, runt)
