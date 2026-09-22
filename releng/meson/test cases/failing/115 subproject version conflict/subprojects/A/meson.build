project('A')

B_dep = subproject('B').get_variable('B_dep')
A_dep = declare_dependency(dependencies: B_dep)
