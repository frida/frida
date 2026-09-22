subdir('Frida_Private')

# Hand-written bespoke sources (message/RPC handlers, auth, actors) and the
# type-agnostic runtime assets. Everything else is emitted into Generated.swift
# by the frida_swift generator below (see `generated_code`).
sources = [
  'AsyncEventSource.swift',
  'AsyncOperation.swift',
  'DeviceListModel.swift',
  'Error+Foundation.swift',
  'Icon+AppKit.swift',
  'Icon+CGImage.swift',
  'Icon+SwiftUI.swift',
  'Icon+UIKit.swift',
  'Icon.swift',
  'JSValue.swift',
  'Marshal.swift',
  'RpcFunction.swift',
  'Runtime.swift',
  'SignalConnection.swift',
  'Version.swift',
]

subdir('GLib')
sources += glib_sources

subdir('JSONGLib')
sources += json_glib_sources

# --- Auto-generated bindings (frida_swift) --------------------------------
#
# Resolve the .gir inputs the same way the Python/Node bindings do.
if frida_core_dep.type_name() == 'internal'
  frida_core_subprj = subproject('frida-core')
  frida_gir = frida_core_subprj.get_variable('core_public_gir')
  glib_gir = frida_core_subprj.get_variable('glib_gir')
  gobject_gir = frida_core_subprj.get_variable('gobject_gir')
  gio_gir = frida_core_subprj.get_variable('gio_gir')
else
  girdir = frida_core_dep.get_variable('frida_girdir')
  frida_gir = girdir / 'Frida-1.0.gir'
  glib_gir = girdir / 'GLib-2.0.gir'
  gobject_gir = girdir / 'GObject-2.0.gir'
  gio_gir = girdir / 'Gio-2.0.gir'
endif

repo_root = meson.project_source_root()

bindgen_env = environment()
bindgen_env.set('PYTHONPATH', [repo_root, repo_root / 'frida-bindgen'])

generated_code = custom_target('swift-binding-code',
  output: ['Generated.swift'],
  input: [
    frida_gir,
    glib_gir,
    gobject_gir,
    gio_gir,
    files(
      repo_root / 'frida_swift' / '__init__.py',
      repo_root / 'frida_swift' / '__main__.py',
      repo_root / 'frida_swift' / 'cli.py',
      repo_root / 'frida_swift' / 'codegen.py',
      repo_root / 'frida_swift' / 'customization.py',
      repo_root / 'frida_swift' / 'loader.py',
      repo_root / 'frida_swift' / 'model.py',
      repo_root / 'frida_swift' / 'assets' / 'compiler_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'device_manager_add_barebone_device.swift',
      repo_root / 'frida_swift' / 'assets' / 'device_manager_add_remote_device.swift',
      repo_root / 'frida_swift' / 'assets' / 'device_manager_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'device_manager_module.swift',
      repo_root / 'frida_swift' / 'assets' / 'endpoint_parameters_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'file_monitor_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'package_install_result_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'package_manager_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'package_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'package_search_result_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'portal_service_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'script_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'web_request_members.swift',
      repo_root / 'frida_swift' / 'assets' / 'web_response_members.swift',
      repo_root / 'frida-bindgen' / 'frida_bindgen_core' / '__init__.py',
      repo_root / 'frida-bindgen' / 'frida_bindgen_core' / 'loader.py',
      repo_root / 'frida-bindgen' / 'frida_bindgen_core' / 'model.py',
      repo_root / 'frida-bindgen' / 'frida_bindgen_core' / 'naming.py',
    ),
  ],
  command: [
    python, '-m', 'frida_swift',
    '--frida-gir=@INPUT0@',
    '--glib-gir=@INPUT1@',
    '--gobject-gir=@INPUT2@',
    '--gio-gir=@INPUT3@',
    '--output-swift=@OUTPUT0@',
  ],
  env: bindgen_env,
)
sources += generated_code

extra_link_args = []
extra_link_depends = []
if host_os_family == 'darwin'
  extra_link_args += ['-Xlinker', '-exported_symbol', '-Xlinker', '_$*']
elif cc.get_argument_syntax() != 'msvc'
  symscript = 'Frida.version'
  extra_link_args += ['-Xlinker', '--version-script', '-Xlinker', meson.current_source_dir() / symscript]
  extra_link_depends += symscript
endif

frida_incdirs = include_directories('.')

frida_lib = shared_library('Frida', sources,
  include_directories: frida_incdirs,
  swift_args: [
    '-swift-version', '5',
    '-enable-library-evolution',
    '-emit-module-interface',
    '-Xcc', '-I' + meson.current_build_dir() / 'Frida_Private',
  ],
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  dependencies: frida_private_dep,
  install: host_os_family != 'darwin',
)

frida_dep = declare_dependency(
  include_directories: frida_incdirs,
  link_with: frida_lib,
)

meson.override_dependency('frida-swift', frida_dep)

if host_os_family == 'darwin'
  custom_target('frida-framework',
    input: [
      frida_lib,
      'Frida.h',
      'Info.plist',
    ],
    output: 'Frida.framework',
    command: [
      python,
      files('generate-framework.py'),
      host_triplet,
      '@OUTPUT@',
      '@INPUT@',
    ],
    install: true,
    install_dir: get_option('prefix') / 'Frameworks',
  )
endif
