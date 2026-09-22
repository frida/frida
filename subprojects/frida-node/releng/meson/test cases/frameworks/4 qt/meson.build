project('qt4, qt5, and qt6 build test', 'cpp',
  # Qt6 requires C++ 17 support
  default_options : ['cpp_std=c++17'])

# Visit the subdir before entering the loop
subdir('mocdep')

qt5_modules = ['Widgets']
qt6_modules = ['Widgets']
foreach qt : ['qt4', 'qt5', 'qt6']
  qt_modules = ['Core', 'Gui']
  if qt == 'qt5'
    qt_modules += qt5_modules
  elif qt == 'qt6'
    qt_modules += qt6_modules
  endif

  # Test that invalid modules are indeed not found
  fakeqtdep = dependency(qt, modules : ['DefinitelyNotFound'], required : false, method : get_option('method'))
  if fakeqtdep.found()
    error('Invalid qt dep incorrectly found!')
  endif

  # Test that partially-invalid modules are indeed not found
  fakeqtdep = dependency(qt, modules : ['Core', 'DefinitelyNotFound'], required : false, method : get_option('method'))
  if fakeqtdep.found()
    error('Invalid qt dep incorrectly found!')
  endif

  # This test should be skipped if the required version of Qt isn't found
  #
  # (In the CI environment, the specified version of Qt is definitely present.
  # An unexpected skip here is treated as a failure, so we are testing that the
  # detection mechanism is able to find Qt.)
  needed_qt = get_option('required').to_lower()
  required = (qt == needed_qt)
  if required
    dep = dependency(qt, modules : ['Core'], required : false, method : get_option('method'))
    if not dep.found()
      error('MESON_SKIP_TEST @0@ not found.'.format(needed_qt))
    endif
  endif

  # Ensure that the "no-Core-module-specified" code branch is hit
  nocoredep = dependency(qt, modules : ['Gui'], required : required, method : get_option('method'))

  # If 'qt' modules are found, test that.
  qtdep = dependency(qt, modules : qt_modules, main : true, private_headers: true, required : required, method : get_option('method'))
  if qtdep.found()
    qtmodule = import(qt)
    assert(qtmodule.has_tools())

    # Test that fetching a variable works and yields a non-empty value
    assert(qtdep.get_variable('prefix', configtool: 'QT_INSTALL_PREFIX') != '')

    # The following has two resource files because having two in one target
    # requires you to do it properly or you get linker symbol clashes.

    prep = qtmodule.preprocess(
      moc_headers : ['mainWindow.h'],           # These need to be fed through the moc tool before use.
      method : get_option('method')
    )
    # XML files that need to be compiled with the uic tol.
    prep += qtmodule.compile_ui(
      sources : 'ui/mainWindow.ui',
      method: get_option('method'),
      preserve_paths: true)

    qtmodule.preprocess(
      ui_files : 'ui/mainWindow.ui',
      method: get_option('method'),
      preserve_paths: true)

    # Resource file(s) for rcc compiler
    extra_cpp_args = []
    if meson.is_unity()
      extra_cpp_args += '-DUNITY_BUILD'
      prep_rcc = qtmodule.preprocess(qt + '_unity_resource', qresources : ['stuff.qrc', 'stuff2.qrc'], method : get_option('method'))
    else
      prep_rcc = qtmodule.preprocess(qresources : ['stuff.qrc', 'stuff2.qrc'], method : get_option('method'))
    endif

    # Test that setting a unique name with a positional argument works
    qtmodule.compile_resources(
      name : qt + 'teststuff',
      sources : files(['stuff.qrc', 'stuff2.qrc']),
      method : get_option('method')
    )

    # Test that passing extra arguments to rcc works
    # qt4-rcc and qt5-rcc take different arguments, for example qt4: ['-compress', '3']; qt5: '--compress=3'
    qtmodule.preprocess(qt + 'testrccarg', qresources : files(['stuff.qrc', 'stuff2.qrc']), rcc_extra_arguments : '--compress=3', method : get_option('method'))

    translations_cpp = qtmodule.compile_translations(qresource: qt+'_lang.qrc')
    # unity builds suck and definitely cannot handle two qrc embeds in one compilation unit
    unityproof_translations = static_library(qt+'unityproof_translations', translations_cpp, dependencies: qtdep)

    extra_cpp_args += '-DQT="@0@"'.format(qt)
    qexe = executable(qt + 'app',
      sources : ['main.cpp', 'mainWindow.cpp', # Sources that don't need preprocessing.
      prep, prep_rcc],
      dependencies : qtdep,
      link_with: unityproof_translations,
      cpp_args: extra_cpp_args,
      gui_app : true)

    # We need a console test application because some test environments
    # do not have an X server.

    translations = qtmodule.compile_translations(ts_files : qt+'core_fr.ts', build_by_default : true)

    qtcore = dependency(qt, modules : 'Core', method : get_option('method'))

    qtcoreapp = executable(qt + 'core', 'q5core.cpp',
      cpp_args: '-DQT="@0@"'.format(qt),
      dependencies : qtcore)

    test(qt + 'test', qtcoreapp)

    # The build system needs to include the cpp files from
    # headers but the user must manually include moc
    # files from sources.
    qtmodule.preprocess(
      moc_extra_arguments : ['-DMOC_EXTRA_FLAG'], # This is just a random macro to test `extra_arguments`
      moc_sources : 'manualinclude.cpp',
      moc_headers : 'manualinclude.h',
      method : get_option('method'),
      dependencies: mocdep,
    )

    manpreprocessed = qtmodule.compile_moc(
      extra_args : ['-DMOC_EXTRA_FLAG'], # This is just a random macro to test `extra_arguments`
      sources : 'manualinclude.cpp',
      headers : 'manualinclude.h',
      method : get_option('method'),
      dependencies: mocdep,
    )

    qtmaninclude = executable(qt + 'maninclude',
      sources : ['manualinclude.cpp', manpreprocessed],
      dependencies : [qtcore, mocdep])

    test(qt + 'maninclude', qtmaninclude)

    # building Qt plugins implies to give include path to moc
    plugin_includes = include_directories('pluginInterface', 'plugin')
    pluginpreprocess = qtmodule.preprocess(
      moc_headers : 'plugin/plugin.h',
      include_directories : plugin_includes
    )
    plugin = library(qt + 'plugin', 'plugin/plugin.cpp', pluginpreprocess,
          include_directories : plugin_includes,
          dependencies : qtcore)

    # implementing Qt interfaces requires passing Qt include paths to moc
    qtinterfacepreprocess = qtmodule.preprocess(
      moc_sources : 'qtinterface.cpp',
      dependencies : qtdep
    )
    qtinterface = library(qt + 'qtinterface',
                          sources : ['qtinterface.cpp', qtinterfacepreprocess],
                          dependencies : qtdep)

    if qt == 'qt5'
      subdir('subfolder')
    endif

    # Check we can apply a version constraint
    accept_versions = ['>=@0@'.format(qtdep.version()), '<@0@'.format(qtdep.version()[0].to_int() + 1)]
    dependency(qt, modules: qt_modules, version: accept_versions, method : get_option('method'))

  endif
endforeach
