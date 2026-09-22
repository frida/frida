project('a', 'c')

good = declare_dependency(link_with: static_library('good', 'g.c'))
bad = declare_dependency(link_args: 'nonexistent.a')
not_found = dependency('invalid', required: false)

source_set = import('sourceset')

sources = source_set.source_set()
sources.add(when: 'YES',  if_false: ['nope.c'])
sources.add(when: 'YES1', if_true: files('a.c'))
subdir('subdir')
sources.add(when: 'NO',   if_true: 'nope.c', if_false: ['f.c'])
sources.add(when: 'NO',   if_true: bad,      if_false: ['f.c'])

sources.add(when: 'YES2', if_true: good)

# dependencies as conditions
sources.add(when: not_found, if_true: 'nope.c')

# test add_all
sources2 = source_set.source_set()
sources2.add(when: 'YES1',  if_true: 'nope.c')
sources.add_all(when: 'NO', if_true: sources2)

# test duplicate items
sources.add(when: 'YES1',   if_true: files('a.c'))

conf1 = {
    'YES': true,
    'YES1': true,
    'YES2': false,
    'NO': false,
}
result1 = sources.apply(conf1)

conf2 = {
    'YES': true,
    'YES1': false,
    'YES2': true,
    'NO': false,
}
result2 = sources.apply(conf2)

# Each target will recompile the objects
executable('first', sources: result1.sources(), dependencies: result1.dependencies())
executable('second', sources: result2.sources(), dependencies: result2.dependencies())

# All target will use the same object files
if meson.is_unity()
  message('Skipping extraction test because this is a Unity build.')
else
  all_objs = static_library('all_objs', sources.all_sources())
  executable('first_via_lib', objects: all_objs.extract_objects(result1.sources()), dependencies: result1.dependencies())
  executable('second_via_lib', objects: all_objs.extract_objects(result2.sources()), dependencies: result2.dependencies())
endif
