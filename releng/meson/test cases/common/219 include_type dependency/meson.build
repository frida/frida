project(
  'dependency include_type',
  ['c', 'cpp'],
  default_options: ['cpp_std=c++11'],
)

dep = dependency('zlib', method: 'pkg-config', required : false)
boost_dep = dependency('boost', modules: ['graph'], include_type : 'system', required: false)

if not dep.found()
  error('MESON_SKIP_TEST zlib was not found')
endif

if not boost_dep.found()
  error('MESON_SKIP_TEST boost was not found')
endif

assert(dep.include_type() == 'preserve', 'include_type must default to "preserve"')

dep_sys = dep.as_system()
assert(dep_sys.include_type() == 'system', 'as_system must return a system dep')

dep2 = dependency('zlib', method: 'pkg-config', include_type : 'system')
assert(dep2.include_type() == 'system', 'include_type must be true when set')

dep2_sys = dep2.as_system('non-system')
assert(dep2_sys.include_type() == 'non-system', 'as_system must set include_type correctly')

sp = subproject('subDep')
sp_dep = sp.get_variable('subDep_dep')
assert(sp_dep.include_type() == 'preserve', 'default is preserve')

sp_dep_sys = sp_dep.as_system('system')
assert(sp_dep_sys.include_type() == 'system', 'changing include_type works')
assert(sp_dep.include_type() == 'preserve', 'as_system must not mutate the original object')

fallback = dependency('sdffgagf_does_not_exist', include_type: 'system', fallback: ['subDep', 'subDep_dep'])
assert(fallback.include_type() == 'system', 'include_type works with dependency fallback')

fallback_empty = dependency('', include_type: 'system', fallback: ['subDep', 'subDep_dep'])
assert(fallback_empty.include_type() == 'system', 'include_type works with empty name dependency fallback')

# Check that PCH works with `include_type : 'system'` See https://github.com/mesonbuild/meson/issues/7167
main_exe = executable('main_exe', 'main.cpp', cpp_pch: 'pch/test.hpp', dependencies: boost_dep)
test('main_test', main_exe)
