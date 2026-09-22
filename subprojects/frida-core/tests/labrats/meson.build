fs = import('fs')

labrat_files = []

if host_os == 'windows'
  labrat_files += [
    'sleeper-windows-x86_64.exe',
    'sleeper-windows-x86.exe',
    'cycler-windows-x86_64.exe',
    'cycler-windows-x86.exe',
    'spawner-windows-x86_64.exe',
    'spawner-windows-x86.exe',
    'simple-agent-windows-x86_64.dll',
    'simple-agent-windows-x86.dll',
    'resident-agent-windows-x86_64.dll',
    'resident-agent-windows-x86.dll',
  ]
endif

if host_os == 'macos'
  labrat_files += [
    'sleeper-macos',
    'sleeper-macos-arm64e',
    'sleeper-macos32',
    'stdio-writer-macos',
    'stdio-writer-macos-arm64e',
    'stdio-writer-macos32',
    'forker-macos',
    'forker-macos-arm64e',
    'forker-macos32',
    'spawner-macos',
    'spawner-macos-arm64e',
    'exception-catcher-macos',
    'exception-catcher-macos-arm64e',
    'simple-agent-macos.dylib',
    'resident-agent-macos.dylib',
  ]
endif

if host_os == 'linux'
  labrat_files += [
    'sleeper-linux-' + host_abi,
    'forker-linux-' + host_abi,
    'spawner-linux-' + host_abi,
    'simple-agent-linux-' + host_abi + '.so',
    'resident-agent-linux-' + host_abi + '.so',
  ]
endif

if host_os == 'ios'
  labrat_files += [
    'sleeper-ios',
    'sleeper-ios32',
    'sleeper-ios64',
    'stdio-writer-ios',
    'stdio-writer-ios32',
    'stdio-writer-ios64',
    'forker-ios',
    'forker-ios32',
    'forker-ios64',
    'spawner-ios',
    'exception-catcher-ios',
    'simple-agent-ios.dylib',
    'resident-agent-ios.dylib',
  ]
endif

if host_os == 'watchos'
  labrat_files += [
    'sleeper-watchos',
    'stdio-writer-watchos',
    'simple-agent-watchos.dylib',
    'resident-agent-watchos.dylib',
  ]
endif

if host_os == 'tvos'
  labrat_files += [
    'sleeper-tvos',
    'stdio-writer-tvos',
    'forker-tvos',
    'spawner-tvos',
    'simple-agent-tvos.dylib',
    'resident-agent-tvos.dylib',
  ]
endif

if host_os == 'android'
  labrat_files += [
    'sleeper-android-' + host_abi,
    'forker-android-' + host_abi,
    'spawner-android-' + host_abi,
    'simple-agent-android-' + host_abi + '.so',
    'resident-agent-android-' + host_abi + '.so',
  ]
endif

if host_os == 'freebsd'
  labrat_files += [
    'sleeper-freebsd-' + host_abi,
    'forker-freebsd-' + host_abi,
    'spawner-freebsd-' + host_abi,
    'simple-agent-freebsd-' + host_abi + '.so',
    'resident-agent-freebsd-' + host_abi + '.so',
  ]
endif

if host_os == 'qnx'
  labrat_files += [
    'sleeper-qnx-arm',
    'forker-qnx-arm',
    'spawner-qnx-arm',
    'simple-agent-qnx-arm.so',
    'resident-agent-qnx-arm.so',
  ]
endif

labrats = []
foreach f : labrat_files
  labrats += fs.copyfile(f)
endforeach
