project('symlinked_subproject', 'c', version : '1.0.0')

dep = declare_dependency(
    sources : 'src.c',
    variables : {
        'datadir': meson.current_source_dir() / 'datadir'
    }
)

subdir('datadir')
