subdir('assets')

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
    'frida.ts',
    'frida_binding.d.ts',
    'frida_binding.c',
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
      'frida_bindgen' / 'assets' / 'codegen_helpers.c',
      'frida_bindgen' / 'assets' / 'codegen_helpers.ts',
      'frida_bindgen' / 'assets' / 'codegen_prototypes.h',
      'frida_bindgen' / 'assets' / 'codegen_types.h',
      'frida_bindgen' / 'assets' / 'customization_facade.exports',
      'frida_bindgen' / 'assets' / 'customization_facade.ts',
      'frida_bindgen' / 'assets' / 'customization_helpers.imports',
      'frida_bindgen' / 'assets' / 'customization_helpers.ts',
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
    '--output-ts=@OUTPUT0@',
    '--output-dts=@OUTPUT1@',
    '--output-c=@OUTPUT2@',
  ],
  env: env,
  install: true,
  install_dir: [
    false,
    pkg_install_dir / 'build' / 'src',
    false,
  ],
)

custom_target('js',
  input: [
    code[0],
    code[1],
  ],
  output: [
    'frida.js',
    'frida.d.ts',
  ],
  command: [
    python,
    files('tsc.py'),
    '@OUTDIR@',
    '@PRIVATE_DIR@',
    npm,
    adjusted_package_json,
    package_lock_json,
    tsconfig_json,
    '@INPUT@',
  ],
  install: true,
  install_dir: pkg_install_dir / 'build' / 'src',
)

frida_binding_sources = [
  code[2],
  files('win_delay_load_hook.c'),
]

if system == 'windows'
  if cc.get_argument_syntax() != 'msvc'
    symfile = 'addon.symbols'
    extra_link_args += '-Wl,--retain-symbols-file,' + meson.current_source_dir() / symfile
    extra_link_depends += files(symfile)
  endif
elif system == 'darwin'
  symlist = 'addon.symbols'
  extra_link_args += '-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist
  extra_link_depends += files(symlist)
else
  symscript = 'addon.version'
  extra_link_args += '-Wl,--version-script,' + meson.current_source_dir() / symscript
  extra_link_depends += files(symscript)
endif
