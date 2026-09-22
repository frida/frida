compiler_sources = files(
  'compiler.vala',
  'backend-glue.c',
)

if have_compiler_backend
  if compiler_backend_mode == 'c-archive'
    output_files = [
      'frida-compiler-backend.a',
      'frida-compiler-backend.h',
    ]
  elif compiler_backend_mode == 'c-shared'
    output_files = [
      'frida-compiler-backend-raw' + shlib_suffix,
      'frida-compiler-backend-raw.h',
    ]
  elif compiler_backend_mode == 'pie'
    output_files = [
      'frida-compiler-backend-raw' + exe_suffix,
    ]
  endif

  compiler_backend = custom_target('frida-compiler-backend',
    input: [
      'go.mod',
      'go.sum',
      'frontend_capi.go',
      'frontend_stdio.go',
      'backend.go',
      'protocol_types.go',
      'backend.version',
      'backend.symbols',
      'tscompiler.go',
      'tsconfig.go',
      'langserver.go',
      'shims.go',
      'package.json',
      'package-lock.json',
    ],
    output: output_files,
    depend_files: go_config,
    command: [
      python,
      files('build-backend.py'),
      go_config,
      meson.current_build_dir(),
      '@PRIVATE_DIR@',
      go,
      npm,
      '@INPUT@',
    ],
  )

  if compiler_backend_mode == 'c-archive'
    backend_libs += compiler_backend[0]
  else
    if compiler_backend_mode == 'c-shared'
      processed_filename = 'frida-compiler-backend' + shlib_suffix
      identity = (host_os_family == 'darwin') ? 'FridaCompilerBackend' : processed_filename
      compiler_backend_binary = custom_target('frida-compiler-backend-binary',
        input: compiler_backend[0],
        output: processed_filename,
        command: post_process + ['shared-library', identity, 'elf-cleaner:off'],
        install: assets_mode == 'installed',
        install_dir: get_option('libdir') / frida_libdir_name / 'plugins',
      )
    elif compiler_backend_mode == 'pie'
      processed_filename = 'frida-compiler-backend' + exe_suffix
      compiler_backend_binary = custom_target('frida-compiler-backend-binary',
        input: compiler_backend[0],
        output: processed_filename,
        command: post_process + ['executable', processed_filename],
        install: assets_mode == 'installed',
        install_dir: get_option('libdir') / frida_libdir_name / 'plugins',
      )
    endif

    if assets_mode == 'installed'
      core_extra_link_depends += compiler_backend_binary
    else
      compiler_data = custom_target('frida-data-compiler',
        input: [
          'compiler.resources',
          compiler_backend_binary,
        ],
        output: [
          'frida-data-compiler.vapi',
          'frida-data-compiler.h',
          'frida-data-compiler.c',
          'frida-data-compiler-blob' + resource_blob_extension,
        ],
        command: [
          resource_compiler_cmd_array,
          '-c', '@INPUT0@',
          '-o', meson.current_build_dir() / 'frida-data-compiler',
          '@INPUT1@',
        ],
      )
      backend_sources += compiler_data
    endif
  endif
endif
