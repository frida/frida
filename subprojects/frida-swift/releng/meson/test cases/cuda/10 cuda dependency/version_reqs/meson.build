exe = executable('prog', 'prog.cc', dependencies: dependency('cuda', version: ['>=8.5', '<10'], required: false, disabler: true))
test('cudatest', exe)
