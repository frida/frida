project('subproject version',
  version : '2.3.4',
  license: 'mylicense')

subproject('a')

liba_dep = dependency('a',
  fallback: ['a', 'liba_dep'],
  version: ['>= 0.30.0', '!= 0.99.0'])
