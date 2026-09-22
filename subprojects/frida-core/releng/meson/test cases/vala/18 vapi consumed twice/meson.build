project('vapi consumed twice', 'vala', 'c')

base_deps = [dependency('glib-2.0'), dependency('gobject-2.0')]

beer = library('beer', 'beer.vala', dependencies : base_deps)
beer_dep = declare_dependency(link_with : beer)

person = library('person', 'person.vala', link_with : beer,
  dependencies : base_deps)
person_dep = declare_dependency(link_with : person, dependencies : [beer_dep])

app = executable('app', 'app.vala',
  dependencies : base_deps + [person_dep, beer_dep])

test('valavapiconsumedtwicetest', app)
