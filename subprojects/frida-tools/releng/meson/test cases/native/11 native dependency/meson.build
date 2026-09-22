project('native dependency fallback', 'c', meson_version : '>= 1.3.0')

d = dependency('made up', native : true)

e = executable('main', 'main.c', dependencies : d, native : true)

test('main', e)
