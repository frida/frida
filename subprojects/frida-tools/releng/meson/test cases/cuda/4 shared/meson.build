project('simple', 'cuda', version : '1.0.0')

subdir('shared')

exe = executable('prog', 'main.cu', dependencies: libkernels)
test('cudatest', exe)
