vapis = [
  'frida-gum-@0@.vapi'.format(api_version),
  'frida-gum-heap-@0@.vapi'.format(api_version),
]

if host_os_family == 'windows'
  vapis += ['frida-gum-windows-@0@.vapi'.format(api_version)]
endif

if host_os_family == 'darwin'
  vapis += ['frida-gum-darwin-@0@.vapi'.format(api_version)]
endif

if host_os_family == 'linux'
  vapis += ['frida-gum-linux-@0@.vapi'.format(api_version)]
endif

if host_os == 'android'
  vapis += ['frida-gum-android-@0@.vapi'.format(api_version)]
endif

if host_os_family == 'freebsd'
  vapis += ['frida-gum-freebsd-@0@.vapi'.format(api_version)]
endif

if have_gumjs
  vapis += [
    'frida-gumjs-@0@.vapi'.format(api_version),
    'frida-gumjs-inspector-@0@.vapi'.format(api_version),
  ]
endif

install_data(vapis, install_dir: get_option('prefix') / get_option('datadir') / 'vala' / 'vapi')
