project('implicit force fallback')

# The dependency 'something' is provided by a subproject. Normally this won't
# use the fallback subproject because required is false. However this unit test
# is configured with wrap_mode=forcefallback and force_fallback_for=something
# in which case we are expecting the fallback to be done.
d = dependency('something', required: false)
assert(d.found())
