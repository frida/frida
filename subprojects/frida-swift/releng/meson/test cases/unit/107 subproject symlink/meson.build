project('foo', 'c')

symlinked_subproject = subproject('symlinked_subproject').get_variable('dep')

executable('foo',
    sources : 'main.c',
    dependencies : symlinked_subproject
)

custom_target(
    input : symlinked_subproject.get_variable('datadir') / 'datafile',
    output : 'datafile_copy',
    command : [find_program('cp.py'), '@INPUT@', '@OUTPUT@'],
    build_always : true
)
