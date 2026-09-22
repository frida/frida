project('install structured data', default_options: ['python.bytecompile=-1'])

install_data(
    'dir1/file1',
    'dir1/file2',
    'dir1/file3',
    'dir2/file1',
    'dir2/file2',
    'dir2/file3',
    'dir3/file1',
    'dir3/file2',
    'dir3/file3',
    install_dir: get_option('datadir'),
    preserve_path: true,
)
subdir('pysrc')
