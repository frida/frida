project('boosttest', 'cpp')

dep = dependency('boost', modules : 'regex', required: false)

assert(dep.found(), 'expected to find a fake version of boost')
assert(dep.version() == '0.1.0', 'expected to find version 0.1.0')
