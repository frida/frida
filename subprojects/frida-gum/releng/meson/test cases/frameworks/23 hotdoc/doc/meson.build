hotdoc = import('hotdoc')

target = hotdoc.generate_doc(
  'foobar',
  c_smart_index: true,
  project_version: '0.1',
  sitemap: 'sitemap.txt',
  index: 'index.md',
  c_sources: files('../../10 gtk-doc/include/foo.h'),
  languages: ['c'],
  install: true,
)

assert(target.config_path() == target.full_path() + '.json',
  'Hotdoc config paths do not match.'
)

assert(hotdoc.has_extensions('search') == true,
  'Extension "search" provided by hotdoc core should always be found')
