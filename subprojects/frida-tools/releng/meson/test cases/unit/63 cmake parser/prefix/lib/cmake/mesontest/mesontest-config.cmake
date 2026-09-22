# This should be enough to satisfy the basic parser
set(MESONTEST_VERSION "1.2.3")
set(MESONTEST_LIBRARIES "foo.so")
set(MESONTEST_INCLUDE_DIR "")
set(MESONTEST_FOUND "TRUE")

## Tests for set() in its various forms

# Basic usage
set(VAR_WITHOUT_SPACES "NoSpaces")
set(VAR_WITH_SPACES "With Spaces")

# test of PARENT_SCOPE, requires a function to have a parent scope obviously...
function(foo)
    set(VAR_WITHOUT_SPACES_PS "NoSpaces" PARENT_SCOPE)
    set(VAR_WITH_SPACES_PS "With Spaces" PARENT_SCOPE)
endfunction(foo)
foo()

# Using set() to unset values
set(VAR_THAT_IS_UNSET "foo")
set(VAR_THAT_IS_UNSET)

# The more advanced form that uses CACHE
# XXX: Why don't we read the type and use that instead of always treat things as strings?
set(CACHED_STRING_NS "foo" CACHE STRING "docstring")
set(CACHED_STRING_WS "foo bar" CACHE STRING "docstring")
set(CACHED_STRING_ARRAY_NS "foo;bar" CACHE STRING "doc string")
set(CACHED_STRING_ARRAY_WS "foo;foo bar;bar" CACHE STRING "stuff" FORCE)

set(CACHED_BOOL ON CACHE BOOL "docstring")

set(CACHED_PATH_NS "foo/bar" CACHE PATH "docstring")
set(CACHED_PATH_WS "foo bar/fin" CACHE PATH "docstring")

set(CACHED_FILEPATH_NS "foo/bar.txt" CACHE FILEPATH "docstring")
set(CACHED_FILEPATH_WS "foo bar/fin.txt" CACHE FILEPATH "docstring")

# Set ENV, we don't support this so it shouldn't be showing up
set(ENV{var}, "foo")


## Tests for set_properties()
# We need something to attach properties too
add_custom_target(MESONTEST_FOO ALL)

set_property(TARGET MESONTEST_FOO PROPERTY FOLDER "value")
set_property(TARGET MESONTEST_FOO APPEND PROPERTY FOLDER "name")
set_property(TARGET MESONTEST_FOO PROPERTY FOLDER "value")
set_property(TARGET MESONTEST_FOO APPEND_STRING PROPERTY FOLDER "name")

set_property(TARGET MESONTEST_FOO PROPERTY FOLDER "value space")
set_property(TARGET MESONTEST_FOO PROPERTY FOLDER "value space")
set_property(TARGET MESONTEST_FOO APPEND PROPERTY FOLDER "name space")
set_property(TARGET MESONTEST_FOO PROPERTY FOLDER "value space")
set_property(TARGET MESONTEST_FOO APPEND_STRING PROPERTY FOLDER "name space")

## Tests for set_target_properties()
set_target_properties(MESONTEST_FOO PROPERTIES FOLDER "value")
set_target_properties(MESONTEST_FOO PROPERTIES FOLDER "value space")
set_target_properties(MESONTEST_FOO PROPERTIES FOLDER "value" OUTPUT_NAME "another value")
set_target_properties(MESONTEST_FOO PROPERTIES FOLDER "value space" OUTPUT_NAME "another value")
set_target_properties(MESONTEST_FOO PROPERTIES FOLDER "value space" OUTPUT_NAME "value")