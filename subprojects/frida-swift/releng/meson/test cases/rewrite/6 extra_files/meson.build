project('rewritetest', 'cpp')

ef1 = ['main.hpp', 'fileA.hpp']
ef2 = files(['fileB.hpp', 'fileC.hpp'])
ef3 = ef1
ef4 = [ef3]
ef5 = []

# Magic comment

exe0 = executable('trivialprog0', 'main.cpp', extra_files : ef1 + ef2)
exe1 = executable('trivialprog1', 'main.cpp', extra_files : ef1)
exe2 = executable('trivialprog2', 'main.cpp', extra_files : [ef2])
exe3 = executable('trivialprog3', 'main.cpp', extra_files : ['main.hpp', 'fileA.hpp'])
exe4 = executable('trivialprog4', 'main.cpp', extra_files : ['main.hpp', ['fileA.hpp']])
exe5 = executable('trivialprog5', 'main.cpp', extra_files : [ef2, 'main.hpp'])
exe6 = executable('trivialprog6', 'main.cpp', extra_files : ef3)
executable('trivialprog7', 'main.cpp', extra_files : ef4)
exe8 = executable('trivialprog8', 'main.cpp', extra_files : [])
exe9 = executable('trivialprog9', 'main.cpp')
exe10 = executable('trivialprog10', 'main.cpp', extra_files : ef5)
