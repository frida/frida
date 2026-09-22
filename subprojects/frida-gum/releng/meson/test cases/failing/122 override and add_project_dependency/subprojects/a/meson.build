project('a', 'c')

dep = dependency('sneaky')

# does not work
add_project_dependencies(dep, language: 'c')
executable('prog', 'prog.c')

# this would work instead:
# executable('prog', 'prog.c', dependencies: dep)
