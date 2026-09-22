project('dependency fallbacks')

# pkg-config has 'libpng' but cmake has 'png' and we have a 'png' subproject
# for platforms that have neither.
d = dependency('libpng', 'png', 'foo')
assert(d.found())

# Check that dependency 'foo' has been implicitly overridden.
d = dependency('foo')
assert(d.found())
