project('empty fallback')

# There is a subproject named 'foo' that overrides that dependency,
# but `fallback: []` should not allow to use it. Same behaviour than with
# `allow_fallback: false`
dependency('foo', fallback: [])
