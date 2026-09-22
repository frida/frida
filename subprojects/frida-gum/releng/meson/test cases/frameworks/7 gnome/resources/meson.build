# There are two tests here, because the 2nd one depends on a version of
# GLib (2.51.1) that is very recent at the time of writing.

simple_gresource = configure_file(
  input : 'simple.gresource.xml',
  output : 'simple-gen.gresource.xml',
  command : [copyfile, '@INPUT@', '@OUTPUT@'])

simple_resources = gnome.compile_resources('simple-resources',
  simple_gresource,
  install_header : true,
  export : true,
  source_dir : '../resources-data',
  c_name : 'simple_resources')

simple_res_exe = executable('simple-resources-test',
  'simple-main.c', simple_resources,
  dependencies: gio)
test('simple resource test', simple_res_exe)

gnome.compile_resources('simple-resources',
  'simple.gresource.xml',
  gresource_bundle: true,
  install: true,
  install_dir: get_option('datadir'),
  source_dir : '../resources-data',
)
test('simple resource test (gresource)', find_program('resources.py'))

if not pretend_glib_old and glib.version() >= '2.52.0'
  # This test cannot pass if GLib version is too old. Generated resource
  # dependencies do not work correctly and Meson will raise an error if the
  # user tries to use either the 'dependencies' kwarg or a gresource file that
  # is itself generated.
  generated_resources = gnome.compile_resources('generated-resources',
    'generated.gresource.xml',
    source_dir : '../resources-data',
    c_name : 'generated_resources',
    dependencies : [res3_txt, res4_txt])

  generated_res_exe = executable('generated-resources-test',
    'generated-main.c', generated_resources,
    dependencies: gio)
  test('generated resource test', generated_res_exe)

  # Test with a CustomTarget
  subdir('generated')

  ct_resources = gnome.compile_resources(
    'ct-resources',
    ct_simple_gresource,
    install_header : true,
    export : true,
    source_dir : '../resources-data',
    c_name : 'simple_resources')

  cti_resources = gnome.compile_resources(
    'cti-resources',
    ct_simple_gresource[0],
    install_header : true,
    export : true,
    source_dir : '../resources-data',
    c_name : 'simple_resources')
endif

# Test build_by_default
gnome.compile_resources('build-resources',
  'simple.gresource.xml',
  gresource_bundle : true,
  build_by_default : true,
  source_dir : '../resources-data',
)
