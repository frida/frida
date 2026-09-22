project(
  'pkgconfig-relocatable-with-absolute-path',
  version : '1.0',
  default_options: [
    'pkgconfig.relocatable=true',
  ])

pkgg = import('pkgconfig')

pkgg.generate(
  name : 'libsimple',
  version : '1.0',
  description : 'A simple pkgconfig.',
  install_dir: get_option('prefix')/'share/misc/pkgconfig',
)
