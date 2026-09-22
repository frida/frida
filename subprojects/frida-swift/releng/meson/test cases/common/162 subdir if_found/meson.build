project('subdir if found')

found_dep = declare_dependency()
not_found_dep = dependency('nonexisting', required : false)

subdir('nonexisting_dir', if_found : not_found_dep)

variable = 3

subdir('subdir', if_found : found_dep)
assert(variable == 5, 'Subdir was not properly entered.')
