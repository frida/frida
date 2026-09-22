project('simple', 'cuda', version : '1.0.0')

subdir('shared')

thread_dep = dependency('threads')
exe = executable('prog', 'main.cu', dependencies: [libkernels, thread_dep])
test('cudatest', exe)
