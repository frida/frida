project('declare dependency', 'c')

subdir('entity')

exe = executable('dep_user', 'main.c',
  dependencies : entity_dep)
test('dep', exe)

# just to make sure [] works as a no-op dep here
executable('dummy', 'main.c',
  dependencies : [entity_dep, []])

# simple case
declare_dependency(dependencies : entity_dep)

# nested deps should be flattened
declare_dependency(dependencies : [entity_dep])
declare_dependency(dependencies : [[entity_dep]])

# check that [] properly works as a no-op dep in declare_dependency() too
declare_dependency(dependencies : [])
declare_dependency(dependencies : [[]])
declare_dependency(dependencies : [entity_dep, []])
declare_dependency(dependencies : [[], entity_dep])
