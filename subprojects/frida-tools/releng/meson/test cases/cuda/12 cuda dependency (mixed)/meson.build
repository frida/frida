project('cuda dependency', 'cpp', 'cuda')

exe = executable('prog', 'prog.cpp', 'kernel.cu', dependencies: dependency('cuda', modules: ['cublas']))
test('cudatest', exe)
