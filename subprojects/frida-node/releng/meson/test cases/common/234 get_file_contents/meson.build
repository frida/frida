project(
  'meson-fs-read-file',
  [],
  version: files('VERSION')
)
fs = import('fs')

assert(fs.read('VERSION').strip() == meson.project_version(), 'file misread')

expected = (
  '∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i), ∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)'
)
assert(
  fs.read('utf-16-text', encoding: 'utf-16').strip() == expected,
  'file was not decoded correctly'
)

# Make sure we handle `files()` objects properly, too
version_file = files('VERSION')

subdir('other')
