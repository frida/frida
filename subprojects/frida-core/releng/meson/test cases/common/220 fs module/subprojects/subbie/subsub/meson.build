assert(fs.exists('subsubfile.txt'), 'Subproject subdir lookup failed.')
assert(fs.is_samepath(meson.project_source_root(), meson.current_source_dir() / '..'), 'is_samepath not detecting same directory')
assert(fs.is_samepath(meson.project_build_root(), meson.current_build_dir() / '..'), 'is_samepath not detecting same directory')
