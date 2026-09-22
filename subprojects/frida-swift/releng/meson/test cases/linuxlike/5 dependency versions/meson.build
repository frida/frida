project('dep versions', 'c', 'cpp')

# Find external dependency without version
zlib = dependency('zlib')
# Find external dependency with version
zlibver = dependency('zlib', version : '>1.0')
assert(zlib.version() == zlibver.version(), 'zlib versions did not match!')
# Find external dependency with conflicting version
assert(zlib.type_name() == 'pkgconfig', 'zlib should be of type "pkgconfig" not ' + zlib.type_name())
zlibver = dependency('zlib', version : '<1.0', required : false)
assert(zlibver.found() == false, 'zlib <1.0 should not be found!')

# Find external dependencies with various version restrictions
dependency('zlib', version : '>=1.0')
dependency('zlib', version : '<=9999')
dependency('zlib', version : '=' + zlib.version())

# Find external dependencies with multiple version restrictions
dependency('zlib', version : ['>=1.0', '<=9999'])
if dependency('zlib', version : ['<=1.0', '>=9999', '=' + zlib.version()], required : false).found()
  error('zlib <=1.0 >=9999 should not have been found')
endif

# Test that a versionless zlib is found after not finding an optional zlib dep with version reqs
zlibopt = dependency('zlib', required : false)
assert(zlibopt.found() == true, 'zlib not found')

# Test https://github.com/mesonbuild/meson/pull/610
dependency('somebrokenlib', version : '>=2.0', required : false)
dependency('somebrokenlib', version : '>=1.0', required : false)

# Search for an external dependency that won't be found, but must later be
# found via fallbacks
somelibnotfound = dependency('somelib1', required : false)
assert(somelibnotfound.found() == false, 'somelibnotfound was found?')
# Find internal dependency without version
somelibver = dependency('somelib1',
  fallback : ['somelibnover', 'some_dep'])
assert(somelibver.type_name() == 'internal', 'somelibver should be of type "internal", not ' + somelibver.type_name())
# Find an internal dependency again with the same name and a specific version
somelib = dependency('somelib2',
  version : '== 0.1',
  fallback : ['somelib', 'some_dep'])
# Find an internal dependency again even if required = false
somelib_reqfalse = dependency('somelib3',
  required: false,
  fallback : ['somelib', 'some_dep'])
assert(somelib_reqfalse.found(), 'somelib should have been found')
# Find an internal dependency again with the same name and incompatible version
somelibver = dependency('somelib4',
  version : '>= 0.3',
  fallback : ['somelibver', 'some_dep'])
# Find an internal dependency again with impossible multi-version
somelibver = dependency('somelib5',
  version : ['>= 0.3', '<0.3'],
  required : false,
  fallback : ['somelibver', 'some_dep'])
assert(not somelibver.found(), 'Dependency should not be found')
# Find somelib again, but with a fallback that will fail because subproject does not exist
somelibfail = dependency('somelib6',
  version : '>= 0.2',
  required : false,
  fallback : ['somelibfail', 'some_dep'])
assert(somelibfail.found() == false, 'somelibfail found via wrong fallback')
# Find somelib again, but with a fallback that will fail because dependency does not exist
somefail_dep = dependency('somelib7',
  version : '>= 0.2',
  required : false,
  fallback : ['somelib', 'somefail_dep'])
assert(somefail_dep.found() == false, 'somefail_dep found via wrong fallback')

# Fallback should only be used if the primary was not found
fallbackzlib_dep = dependency('zlib',
  fallback : ['fakezlib', 'fakezlib_dep'])
assert(fallbackzlib_dep.type_name() == 'pkgconfig', 'fallbackzlib_dep should be of type "pkgconfig", not ' + fallbackzlib_dep.type_name())
# Check that the above dependency was pkgconfig because the fallback wasn't
# checked, not because the fallback didn't work
fakezlib_dep = dependency('fakezlib',
  fallback : ['fakezlib', 'fakezlib_dep'])
assert(fakezlib_dep.type_name() == 'internal', 'fakezlib_dep should be of type "internal", not ' + fakezlib_dep.type_name())

# Verify that once we got a system dependency, we won't fallback if a newer
# version is requested.
d = dependency('zlib', version: '>= 999',
  fallback : ['nonexistent', 'fakezlib_dep'],
  required: false)
assert(not d.found(), 'version should not match and it should not fallback')

# Check that you can find a dependency by not specifying a version after not
# finding it by specifying a version. We add `static: true` here so that the
# previously cached zlib dependencies don't get checked.
dependency('zlib', static : true, version : '>=8000', required : false)
dependency('zlib', static : true)

# Check that you can find a dependency by specifying a correct version after
# not finding it by specifying a wrong one. We add `method: pkg-config` here so that
# the previously cached zlib dependencies don't get checked.
bzip2 = dependency('zlib', method : 'pkg-config', version : '>=9000', required : false)
bzip2 = dependency('zlib', method : 'pkg-config', version : '>=1.0')

if meson.is_cross_build()
  # Test caching of native and cross dependencies
  # https://github.com/mesonbuild/meson/issues/1736
  cross_prefix = dependency('zlib').get_pkgconfig_variable('prefix')
  native_prefix = dependency('zlib', native : true).get_pkgconfig_variable('prefix')
  assert(cross_prefix != '', 'cross zlib prefix is not defined')
  assert(native_prefix != '', 'native zlib prefix is not defined')
  assert(native_prefix != cross_prefix, 'native prefix == cross_prefix == ' + native_prefix)
endif

objc_found = add_languages('objc', required : false)

foreach d : ['sdl2', 'gnustep', 'wxwidgets', 'gl', 'python3', 'boost', 'gtest', 'gmock', 'valgrind']
  if d == 'gnustep' and not objc_found
    message('Skipping gnustep because no ObjC compiler found')
  else
    dep = dependency(d, required : false)
    if dep.found()
      dep.version()
    endif
  endif
endforeach
