project(
  'pkgconfig-not-relocatable-outside-prefix',
  version : '1.0',
  default_options: [
    'pkgconfig.relocatable=true',
  ])

pkgg = import('pkgconfig')

# A drive letter is needed on windows for this to be an absolute path.
if host_machine.system() == 'windows'
  install_dir = 'C:/opt/lib/pkgconfig'
else
  install_dir = '/opt/lib/pkgconfig'
endif

pkgg.generate(
  name : 'libsimple',
  version : '1.0',
  description : 'A simple pkgconfig file.',
  install_dir: install_dir,
)
