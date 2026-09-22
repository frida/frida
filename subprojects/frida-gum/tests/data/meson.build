if host_os_family != 'windows'
  test_function_filenames = []

  test_function_libraries = [
    'targetfunctions',
    'specialfunctions',
  ]

  if host_os_family == 'darwin'
    shlib_extension = 'dylib'
  else
    shlib_extension = 'so'
  endif

  if host_machine.endian() == 'big'
    if host_abi == 'arm' or host_abi == 'armhf'
      host_abi = 'armbe8'
    elif host_abi == 'arm64'
      host_abi = 'arm64be'
    endif
  endif

  foreach l : test_function_libraries
    name = '@0@-@1@-@2@.@3@'.format(l, host_os, host_abi, shlib_extension)
    test_function_filenames += [name]
  endforeach

  if host_os == 'linux' and host_arch == 'x86_64'
    test_function_filenames += [
      'module-registry-target-relocated-phdr-linux-x86_64.so',
    ]
  endif

  test_data = custom_target('gum-tests-data',
    input: test_function_filenames,
    output: test_function_filenames,
    command: ['cp', '@INPUT@', '@OUTDIR@'],
  )
  test_data_stamp = custom_target('gum-tests-data-stamp',
    input: [test_data],
    output: '.stamp',
    command: ['touch', '@OUTPUT@'],
  )

  shared_module('prebuiltcmodule', 'prebuiltcmodule.c',
    name_prefix: '',
  )

  shared_module('module-registry-target', 'module-registry-target.c',
    name_prefix: '',
  )
else
  test_data_stamp = []
endif
