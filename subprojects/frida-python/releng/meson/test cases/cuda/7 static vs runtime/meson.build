project('static msvc runtime', 'cuda', version : '1.0.0', default_options : ['b_vscrt=mtd'])

exe = executable('prog', 'main.cu')
test('cudatest', exe)
