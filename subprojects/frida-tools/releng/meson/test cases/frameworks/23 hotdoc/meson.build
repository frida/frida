project('hotdoc', 'c')

hotdoc = find_program('hotdoc', required: false)
if not hotdoc.found()
  error('MESON_SKIP_TEST hotdoc not found.')
endif

subdir('doc')

assert(hotdoc.has_extensions(['gi-extension']) == true,
  'GI extension should always be found.')

assert(hotdoc.has_extensions(['gi-extension', 'no-way-you-exist-extension']) == false,
  'A hotdoc extension called "no-way-you-exist-extension" should never be found.')
