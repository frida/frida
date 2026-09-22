sources = files('com/mesonbuild/Resources.java')

resources = jar(
  meson.project_name(),
  sources,
  main_class: 'com.mesonbuild.Resources',
  java_resources: structured_sources(
    files('resources/resource1.txt'),
    {
      'subdir': files('resources/subdir/resource2.txt'),
    }
  )
)
