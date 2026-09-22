project('61 cmake parser')

dep = dependency('mesontest')

# Test a bunch of variations of the set() command
assert(dep.get_variable(cmake : 'VAR_WITHOUT_SPACES') == 'NoSpaces', 'set() without spaces incorrect')
assert(dep.get_variable(cmake : 'VAR_WITH_SPACES') == 'With Spaces', 'set() with spaces incorrect')

assert(dep.get_variable(cmake : 'VAR_WITHOUT_SPACES_PS') == 'NoSpaces', 'set(PARENT_SCOPE) without spaces incorrect')
assert(dep.get_variable(cmake : 'VAR_WITH_SPACES_PS') == 'With Spaces', 'set(PARENT_SCOPE) with spaces incorrect')

assert(dep.get_variable(cmake : 'VAR_THAT_IS_UNSET', default_value : 'sentinel') == 'sentinel', 'set() to unset is incorrect')
assert(dep.get_variable(cmake : 'CACHED_STRING_NS') == 'foo', 'set(CACHED) without spaces is incorrect')
assert(dep.get_variable(cmake : 'CACHED_STRING_WS') == 'foo bar', 'set(CACHED STRING) with spaces is incorrect')
assert(dep.get_variable(cmake : 'CACHED_STRING_ARRAY_NS') == 'foo;bar', 'set(CACHED STRING) without spaces is incorrect')
assert(dep.get_variable(cmake : 'CACHED_STRING_ARRAY_WS') == 'foo;foo bar;bar', 'set(CACHED STRING[]) with spaces is incorrect')

# We don't support this, so it should be unset.
assert(dep.get_variable(cmake : 'ENV{var}', default_value : 'sentinel') == 'sentinel', 'set(ENV) should be ignored')
