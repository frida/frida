project(
  'dependency get_variable',
  ['c', 'cpp'],
)

# Just some string that nothing should return
default = 'asufoiqwjtl;adjfbpiuqwoehtl;ajdfl;ghal;sdjg'

dep = dependency('zlib', method: 'pkg-config', required : false)
if not dep.found()
  warning('Skipping pkg-config tests as zlib is not available or is not pkg-config')
else
  # Test for regular pkg-config
  # We don't know what the value will be, but we know it should be the same
  dep = dependency('zlib', method : 'pkg-config')
  assert(dep.get_pkgconfig_variable('prefix') == dep.get_variable(pkgconfig : 'prefix'),
        'Got different values from get_pkgconfig_variable and get_variable(pkgconfig: )')
  assert(dep.get_variable(pkgconfig : default, default_value : default) == default,
        'pkg-config didn\'t get default when we should have.')
  assert(dep.get_variable(pkgconfig : 'prefix', default_value : default) != default,
        'pkg-config got default when we shouldn\'t have.')
  assert(dep.get_variable(pkgconfig : 'pkgvarnotfound', default_value : '') == '')
endif

dep_ct = dependency('llvm', method : 'config-tool', required : false)
if not dep_ct.found()
  warning('Skipping config-tool tests as llvm is not available or llvm-config was not found.')
else
  assert(dep_ct.get_configtool_variable('has-rtti') == dep_ct.get_variable(configtool : 'has-rtti'),
        'Got different values from get_configtool_variable and get_variable(configtool: )')
  assert(dep_ct.get_variable(configtool : default, default_value : default) == default,
        'config-tool didn\'t get default when we should have.')
  assert(dep_ct.get_variable(configtool : 'has-rtti', default_value : default) != default,
        'config-tool got default when we shouldn\'t have.')
endif

dep_cm = dependency('llvm', method : 'cmake', required : false)
if not dep_cm.found()
  warning('Skipping cmake tests as llvm is not available via the cmake finder.')
else
  if dep_ct.found()
    assert((dep_cm.get_variable(cmake : 'LLVM_ENABLE_RTTI') == 'ON') == (dep_ct.get_variable(configtool : 'has-rtti') == 'YES'),
          'RTTI information for cmake and config tools disagree')
  endif
  assert(dep_cm.get_variable(cmake : default, default_value : default) == default,
        'cmake didn\'t get default when we should have.')
  assert(dep_cm.get_variable(cmake : 'LLVM_ENABLE_RTTI', default_value : default) != default,
        'cmake config-tool got default when we shouldn\'t have.')
endif

idep = declare_dependency(variables : {'foo' : 'value'})
assert(idep.get_variable(pkgconfig : 'foo', cmake : 'foo', configtool : 'foo',
                         internal : 'foo', default_value : default) == 'value',
       'internal got default when it shouldn\'t have.')
assert(idep.get_variable(pkgconfig : 'foo', cmake : 'foo', configtool : 'foo',
                         internal : 'bar', default_value : default) == default,
       'internal didn\'t default when it should have.')

idep = declare_dependency()
assert(idep.get_variable(pkgconfig : 'foo', cmake : 'foo', configtool : 'foo',
                         default_value : default) == default,
       'something went wrong with an InternalDependency with no variables.')

idep = declare_dependency(variables : ['foo=value'])
assert(idep.get_variable(internal: 'foo') == 'value')
assert(idep.get_variable('foo') == 'value')
assert(idep.get_variable('invalid', internal: 'foo') == 'value')
