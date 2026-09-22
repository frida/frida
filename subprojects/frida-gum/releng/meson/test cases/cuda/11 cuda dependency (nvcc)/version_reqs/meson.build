exe = executable('prog', 'prog.cu', dependencies: dependency('cuda', version: ['>=10.1'], required: false, disabler: true))
test('cudatest', exe)
