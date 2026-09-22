project('foo')

# It is fine to include the root source dir
include_directories('.')
subproject('sub')

# This is here rather than in failing because this needs a
# transition period to avoid breaking existing projects.
# Once this becomes an error, move this under failing tests.

inc = include_directories('subprojects/sub/include')
