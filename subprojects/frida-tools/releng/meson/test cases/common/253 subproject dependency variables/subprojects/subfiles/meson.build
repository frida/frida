project('dependency variable resource')

files_dep = declare_dependency(
    variables: [
      'pkgdatadir=@0@/subdir'.format(meson.current_source_dir()),
      'pkgdatadir2=@0@/subdir2'.format(meson.current_source_dir()),
      'pkgdatadir3=@0@'.format(meson.current_source_dir()),
    ]
)

install_data('subdir/foo.c', install_dir: get_option('datadir') / 'subdir')
install_subdir('subdir2', install_dir: get_option('datadir'))
install_data('foo.c', install_dir: get_option('datadir'))

import('pkgconfig').generate(
    name: 'depvar_resource',
    description: 'Get a resource file from pkgconfig or a subproject',
    version: '0.1',
    variables: [
      'pkgdatadir=${datadir}/subdir',
      'pkgdatadir2=${datadir}/subdir2',
    ],
    uninstalled_variables: [
      'pkgdatadir=@0@/subdir'.format(meson.current_source_dir()),
      'pkgdatadir2=@0@/subdir2'.format(meson.current_source_dir()),
      'pkgdatadir3=@0@'.format(meson.current_source_dir()),
    ],
    dataonly: true,
)
