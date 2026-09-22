scripts = [
  ['frida', 'repl'],
  ['frida-apk', 'apk'],
  ['frida-compile', 'compiler'],
  ['frida-create', 'create'],
  ['frida-discover', 'discover'],
  ['frida-itrace', 'itracer'],
  ['frida-join', 'join'],
  ['frida-kill', 'kill'],
  ['frida-ls', 'ls'],
  ['frida-ls-devices', 'lsd'],
  ['frida-pm', 'pm'],
  ['frida-ps', 'ps'],
  ['frida-pull', 'pull'],
  ['frida-push', 'push'],
  ['frida-rm', 'rm'],
  ['frida-strace', 'stracer'],
  ['frida-trace', 'tracer'],
]

common_cdata = configuration_data()
common_cdata.set('PYTHON', python.full_path())
common_cdata.set('pythondir', python.get_install_dir())

foreach s : scripts
  cdata = configuration_data()
  cdata.merge_from(common_cdata)
  cdata.set('module', s.get(1))

  generated_script = configure_file(
    input: 'script.in',
    output: s.get(0),
    configuration: cdata,
  )

  install_data(generated_script,
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x',
  )
endforeach
