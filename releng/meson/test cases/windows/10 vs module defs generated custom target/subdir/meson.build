make_def = find_program('make_def.py')

def_file = custom_target('gen_def',
        command: [make_def, '@OUTPUT@'],
        output: 'somedll.def')

shlib = shared_library('somedll', 'somedll.c', vs_module_defs: def_file)

shared_library('somedll2', 'somedll.c', vs_module_defs: def_file[0])
