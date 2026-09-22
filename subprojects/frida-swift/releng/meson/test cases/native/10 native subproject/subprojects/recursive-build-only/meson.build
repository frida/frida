project('recursive-build-only', 'c')

assert(meson.is_cross_build() == false, 'this project should never be considered a cross target')

dep = dependency('recursive-both')

exe = executable('main', 'main.c', dependencies : dep)

meson.override_find_program('build-only-tool', exe)
