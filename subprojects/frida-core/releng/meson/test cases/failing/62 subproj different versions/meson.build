project('super', 'c')

# A will use version 1 of C
a_dep = dependency('a', fallback: ['a', 'a_dep'])

# B will fail because it requests version 2 of C
b_dep = dependency('b', fallback: ['b', 'b_dep'])

main = executable('main', files('main.c'), dependencies: [a_dep, b_dep])
