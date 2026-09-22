backend_sources += files(
  'barebone-host-session.vala',
  'emulator-instrumentation.vala',
  'agent-connection.vala',
  'script.vala',
  'machine.vala',
  'allocator.vala',
  'callback.vala',
  'memory-scanner.vala',
  'interceptor.vala',
  'c-module.vala',
  'rust-module.vala',
  'console.vala',
  'layout.vala',
  'elf.vala',
  'img4.vala',
  'lzfse.vala',
  'qmp-client.vala',
  'kernel-relocation.vala',
  'kernel-flavor.vala',
  'pe.vala',
  'linux.vala',
  'linux-kallsyms.vala',
  'win9x.vala',
  'xnu.vala',
  'win9x-service-names.vala',
  'winnt.vala',
  'x86-paging.vala',
  'x86-inline-hook.vala',
  'vz-stub.vala',
  'parallels-stub.vala',
  'arch-x86' / 'machine.vala',
  'arch-x86_64' / 'machine.vala',
  'arch-arm' / 'machine.vala',
  'arch-arm64' / 'machine.vala',
  'arch-unknown' / 'machine.vala',
)
if host_os_family == 'darwin'
  backend_sources += files('vz-physical-memory.vala')
  backend_vala_args += '--pkg=frida-gum-darwin-1.0'
endif
barebone_script_runtime = custom_target('frida-barebone-script-runtime',
  input: [
    'package.json',
    'package-lock.json',
    'script-runtime' / 'console.ts',
    'script-runtime' / 'entrypoint.ts',
    'script-runtime' / 'gdb.ts',
    'script-runtime' / 'hexdump.ts',
    'script-runtime' / 'message-dispatcher.ts',
    'script-runtime' / 'primitives.ts',
  ],
  output: [
    'script-runtime.js',
  ],
  command: [
    python,
    compile_js_agent,
    '@OUTPUT@',
    '@PRIVATE_DIR@',
    npm,
    '@INPUT@',
  ],
)
backend_sources += custom_target('frida-data-barebone',
  input: [
    'barebone.resources',
    barebone_script_runtime,
    'helpers/memory-scanner-arm64.elf',
    'emulator-gdbstub-shim.js',
  ],
  output: [
    'frida-data-barebone.vapi',
    'frida-data-barebone.h',
    'frida-data-barebone.c',
    'frida-data-barebone-blob' + resource_blob_extension,
  ],
  command: [
    resource_compiler_cmd_array,
    '-c', '@INPUT0@',
    '-o', meson.current_build_dir() / 'frida-data-barebone',
    '@INPUT1@',
    '@INPUT2@',
    '@INPUT3@',
  ],
)

backend_vala_args += gum_vala_args
backend_vala_args_private += '--pkg=quickjs'
