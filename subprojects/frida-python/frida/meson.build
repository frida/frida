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

env = environment()
env.set('PYTHONPATH', [meson.current_source_dir(), meson.project_source_root() / 'frida-bindgen'])

code = custom_target('binding-code',
  output: [
    '__init__.py',
    '_frida.pyi',
    'extension.c',
    'aio.py',
  ],
  input: [
    frida_gir,
    glib_gir,
    gobject_gir,
    gio_gir,
    files(
      'frida_bindgen' / '__init__.py',
      'frida_bindgen' / '__main__.py',
      'frida_bindgen' / 'cli.py',
      'frida_bindgen' / 'codegen.py',
      'frida_bindgen' / 'customization.py',
      'frida_bindgen' / 'loader.py',
      'frida_bindgen' / 'model.py',
      'frida_bindgen' / 'assets' / 'codegen_gobject_globals.c',
      'frida_bindgen' / 'assets' / 'codegen_gobject_methods.c',
      'frida_bindgen' / 'assets' / 'codegen_gobject_prototypes.h',
      'frida_bindgen' / 'assets' / 'codegen_macros.h',
      'frida_bindgen' / 'assets' / 'codegen_structs.h',
      'frida_bindgen' / 'assets' / 'codegen_typedefs.h',
      'frida_bindgen' / 'assets' / 'facade_toplevel.py',
      'frida_bindgen' / 'assets' / 'facade_toplevel_aio.py',
      'frida_bindgen' / 'assets' / 'facade_cancellable.py',
      'frida_bindgen' / 'assets' / 'facade_cancellable_aio.py',
      'frida_bindgen' / 'assets' / 'facade_cancellable_helpers.py',
      'frida_bindgen' / 'assets' / 'facade_cancellable_helpers_aio.py',
      'frida_bindgen' / 'assets' / 'facade_device_members.py',
      'frida_bindgen' / 'assets' / 'facade_device_members_aio.py',
      'frida_bindgen' / 'assets' / 'facade_device_manager_members.py',
      'frida_bindgen' / 'assets' / 'facade_device_manager_members_aio.py',
      'frida_bindgen' / 'assets' / 'facade_bus.py',
      'frida_bindgen' / 'assets' / 'facade_bus_aio.py',
      'frida_bindgen' / 'assets' / 'facade_portal.py',
      'frida_bindgen' / 'assets' / 'facade_portal_aio.py',
      'frida_bindgen' / 'assets' / 'facade_rpc_helpers.py',
      'frida_bindgen' / 'assets' / 'facade_rpc_helpers_aio.py',
      'frida_bindgen' / 'assets' / 'facade_rpc_members.py',
      'frida_bindgen' / 'assets' / 'facade_rpc_members_aio.py',
      'frida_bindgen' / 'assets' / 'facade_iostream.py',
      'frida_bindgen' / 'assets' / 'facade_iostream_aio.py',
      'frida_bindgen' / 'assets' / 'facade_interface.py',
      'frida_bindgen' / 'assets' / 'facade_interface_aio.py',
      'frida_bindgen' / 'assets' / 'codegen_endpoint_parameters.c',
    ),
    files(
      meson.project_source_root() / 'frida-bindgen' / 'frida_bindgen_core' / '__init__.py',
      meson.project_source_root() / 'frida-bindgen' / 'frida_bindgen_core' / 'loader.py',
      meson.project_source_root() / 'frida-bindgen' / 'frida_bindgen_core' / 'model.py',
      meson.project_source_root() / 'frida-bindgen' / 'frida_bindgen_core' / 'naming.py',
    ),
  ],
  command: [
    python, '-m', 'frida_bindgen',
    '--frida-gir=@INPUT0@',
    '--glib-gir=@INPUT1@',
    '--gobject-gir=@INPUT2@',
    '--gio-gir=@INPUT3@',
    '--output-py=@OUTPUT0@',
    '--output-pyi=@OUTPUT1@',
    '--output-c=@OUTPUT2@',
    '--output-aio=@OUTPUT3@',
  ],
  env: env,
  install: true,
  install_dir: [
    python.get_install_dir() / 'frida',
    python.get_install_dir() / 'frida',
    false,
    python.get_install_dir() / 'frida',
  ]
)

py_sources = [
  'py.typed',
]
python.install_sources(py_sources, subdir: 'frida', pure: false)

extra_link_args = []
if host_os_family == 'darwin'
  extra_link_args += '-Wl,-exported_symbol,_PyInit__frida'
elif host_os_family != 'windows'
  extra_link_args += '-Wl,--version-script,' + meson.current_source_dir() / 'extension.version'
endif

extension = python.extension_module('_frida', code[2],
  limited_api: '3.7',
  c_args: frida_component_cflags,
  link_args: extra_link_args,
  dependencies: [python_dep, frida_core_dep, os_deps],
  install: true,
  subdir: 'frida',
)
