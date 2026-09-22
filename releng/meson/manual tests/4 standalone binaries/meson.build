project('myapp', 'cpp')

sdl = dependency('sdl2', required : host_machine.system() != 'windows')

if meson.get_compiler('cpp').get_id() != 'msvc'
  add_global_arguments('-std=c++11', language : 'cpp')
endif

if host_machine.system() == 'darwin'
  install_data('myapp.sh',
  install_dir : 'Contents/MacOS')

  install_data('myapp.icns',
  install_dir : 'Contents/Resources')

  install_data('Info.plist',
  install_dir : 'Contents')

  meson.add_install_script('osx_bundler.sh')
endif

if host_machine.system() == 'linux'
  install_data('myapp.sh', install_dir : '.')
  meson.add_install_script('linux_bundler.sh')
endif

extra_link_args = []

if host_machine.system() == 'windows'
  str = '-I@0@/@1@'.format(meson.current_build_dir(), 'SDL2-2.0.3/include')
  add_global_arguments(str, language : 'cpp')
  extra_link_args = ['/SUBSYSTEM:CONSOLE', 'SDL2main.lib', 'SDL2.lib']
endif

prog = executable('myapp', 'myapp.cpp',
dependencies : sdl,
link_args : extra_link_args,
install : true)
