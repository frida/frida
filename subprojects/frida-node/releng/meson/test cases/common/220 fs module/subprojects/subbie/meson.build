project('subbie')

fs = import('fs')

assert(fs.exists('subprojectfile.txt'), 'Subproject root file not found.')
assert(fs.is_samepath(meson.project_source_root(), meson.current_source_dir()), 'is_samepath not detecting same directory')
assert(fs.is_samepath(meson.project_build_root(), meson.current_build_dir()), 'is_samepath not detecting same directory')
assert(fs.is_samepath(meson.global_source_root(), meson.current_source_dir() / '../..'), 'is_samepath not detecting same directory')
assert(fs.is_samepath(meson.global_build_root(), meson.current_build_dir() / '../..'), 'is_samepath not detecting same directory')

subdir('subsub')
