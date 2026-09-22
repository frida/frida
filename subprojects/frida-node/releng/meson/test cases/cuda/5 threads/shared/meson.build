libkernels = shared_library('kernels', 'kernels.cu',
                            cuda_args: ['-DTAG_IS_SHARED=1', '-DTAG_IS_BUILDING=1'],
                            gnu_symbol_visibility: 'hidden')
libkernels = declare_dependency(compile_args: ['-DTAG_IS_SHARED=1'],
                                link_with:    libkernels)
