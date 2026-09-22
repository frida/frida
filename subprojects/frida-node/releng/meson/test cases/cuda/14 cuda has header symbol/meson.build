project('cuda has header symbol', 'cuda')

cuda = meson.get_compiler('cuda')

# C checks
assert (cuda.has_header_symbol('stdio.h', 'int'), 'base types should always be available')
assert (cuda.has_header_symbol('stdio.h', 'printf'), 'printf function not found')
assert (cuda.has_header_symbol('stdio.h', 'FILE'), 'FILE structure not found')
assert (cuda.has_header_symbol('limits.h', 'INT_MAX'), 'INT_MAX define not found')
assert (not cuda.has_header_symbol('limits.h', 'guint64'), 'guint64 is not defined in limits.h')
assert (not cuda.has_header_symbol('stdlib.h', 'FILE'), 'FILE structure is defined in stdio.h, not stdlib.h')
assert (not cuda.has_header_symbol('stdlol.h', 'printf'), 'stdlol.h shouldn\'t exist')
assert (not cuda.has_header_symbol('stdlol.h', 'int'), 'shouldn\'t be able to find "int" with invalid header')

# C++ checks
assert (cuda.has_header_symbol('iostream', 'std::iostream'), 'iostream not found in iostream.h')
assert (cuda.has_header_symbol('vector', 'std::vector'), 'vector not found in vector.h')
assert (not cuda.has_header_symbol('limits.h', 'std::iostream'), 'iostream should not be defined in limits.h')

# CUDA checks
assert (cuda.has_header_symbol('cuda.h', 'CUDA_VERSION'), 'CUDA_VERSION not found in cuda.h')
assert (not cuda.has_header_symbol('cuda.h', 'cublasSaxpy'), 'cublasSaxpy is defined in cublas.h, not cuda.h')
if cuda.version().version_compare('>=4.0')
    assert (cuda.has_header_symbol('thrust/device_vector.h', 'thrust::device_vector'), 'thrust::device_vector not found')
    assert (not cuda.has_header_symbol('thrust/fill.h', 'thrust::sort'), 'thrust::sort should not be defined in thrust/fill.h')
endif
