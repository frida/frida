project('frida-qml', 'cpp',
  version: run_command(find_program('python3'), files('tools' / 'detect-version.py'),
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.1.0',
  default_options: [
    'cpp_std=c++17',
  ],
)

cpp = meson.get_compiler('cpp')

qt_dep = dependency('qt6',
  modules: [
    'Core',
    'Gui',
    'Network',
    'OpenGL',
    'Qml',
    'QmlModels',
    'Quick',
  ],
)
frida_core_dep = dependency('frida-core-1.0', default_options: [
  'frida_version=' + meson.project_version(),
  'compiler_backend=disabled',
])

add_project_arguments(
  '-DQT_PLUGIN',
  '-DQT_NO_KEYWORDS',
  '-DQT_NO_DEBUG',
  '-DQT_CORE_LIB',
  '-DQT_GUI_LIB',
  '-DQT_NETWORK_LIB',
  '-DQT_OPENGL_LIB',
  '-DQT_QML_LIB',
  '-DQT_QMLINTEGRATION_LIB',
  '-DQT_QMLMODELS_LIB',
  '-DQT_QUICK_LIB',
  language: 'cpp',
)

if cpp.get_id() == 'clang'
  add_project_arguments('-Wno-error=implicit-function-declaration', language: 'cpp')
endif

subdir('src')
