project('rewritetest', 'cpp')

src1 = ['main.cpp', 'fileA.cpp']
src2 = files(['fileB.cpp', 'fileC.cpp'])
src3 = src1
src4 = [src3]

# Magic comment

exe0 = executable('trivialprog0', src1 + src2)
exe1 = executable('trivialprog1', src1)
exe2 = executable('trivialprog2', [src2])
exe3 = executable('trivialprog3', ['main.cpp', 'fileA.cpp'])
exe4 = executable('trivialprog4', ['main.cpp', ['fileA.cpp']])
exe5 = executable('trivialprog5', [src2, 'main.cpp'])
exe6 = executable('trivialprog6', 'main.cpp', 'fileA.cpp')
exe7 = executable('trivialprog7', 'fileB.cpp', src1, 'fileC.cpp')
exe8 = executable('trivialprog8', src3)
executable('trivialprog9', src4)
