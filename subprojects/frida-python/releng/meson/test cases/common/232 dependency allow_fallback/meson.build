project('subproject fallback')

foob_dep = dependency('foob', allow_fallback: true, required: false)
assert(foob_dep.found())

# Careful!  Once a submodule has been triggered and it has
# overridden the dependency, it sticks.
foob_dep = dependency('foob', allow_fallback: false, required: false)
assert(foob_dep.found())

foob3_dep = dependency('foob3', allow_fallback: false, required: false)
assert(not foob3_dep.found())
