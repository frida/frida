project('dep fallback', 'c')

bob = dependency('boblib', fallback : ['boblib', 'bob_dep'], required: false,
  default_options : 'warning_level=1')
if not bob.found()
    error('Bob is actually needed')
endif

# boblib subproject exists, but bobinc is not a dependency variable
sita = dependency('sitalib', fallback : ['boblib', 'bobinc'], required: false)
assert(not sita.found())
# boblib subproject exists, but sita_dep doesn't exist
sita = dependency('sitalib', fallback : ['boblib', 'sita_dep'], required: false)
assert(not sita.found())
# boblib has been configured so zlib cannot be searched on the system
zlib = dependency('zlib', fallback : ['boblib', 'notfound_dep'], required: false)
assert(not zlib.found())
# boblib has been configured so zlib cannot be searched on the system.
# Not variable name provided and the subproject does not override zlib.
zlib = dependency('zlib', fallback : 'boblib', required: false)
assert(not zlib.found())

# jimmylib subproject doesn't exist
jimmy = dependency('jimmylib', fallback : ['jimmylib', 'jimmy_dep'], required: false)
# dummylib subproject fails to configure
dummy = dependency('dummylib', fallback : ['dummylib', 'dummy_dep'], required: false)

gensrc_py = find_program('gensrc.py')
gensrc = custom_target('gensrc.c',
  input : 'tester.c',
  output : 'gensrc.c',
  command : [gensrc_py, '@INPUT@', '@OUTPUT@'])

exe = executable('bobtester',
  [gensrc],
  dependencies : bob)

test('bobtester', exe)
