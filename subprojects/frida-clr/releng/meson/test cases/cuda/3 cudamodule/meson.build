project('cudamodule', 'cuda', version : '1.0.0')

nvcc = meson.get_compiler('cuda')
cuda = import('unstable-cuda')

arch_flags     = cuda.nvcc_arch_flags(nvcc.version(), 'Auto', detected: ['6.0'])
arch_readable  = cuda.nvcc_arch_readable(nvcc.version(), 'Auto', detected: ['6.0'])
driver_version = cuda.min_driver_version(nvcc.version())

message('NVCC version:   ' + nvcc.version())
message('NVCC flags:     ' + ' '.join(arch_flags))
message('NVCC readable:  ' + ' '.join(arch_readable))
message('Driver version: >=' + driver_version)

exe = executable('prog', 'prog.cu', cuda_args: arch_flags)
test('cudatest', exe)


#
# Assert Series
#

# Sanity test.
assert(' '.join(cuda.nvcc_arch_flags('11.1', '8.6')) ==
       '-gencode arch=compute_86,code=sm_86')

# CUDA Toolkit too old, flag filtered out.
assert(' '.join(cuda.nvcc_arch_flags('11.0', '8.6')) ==
       '')

# Named architectures.
assert(' '.join(cuda.nvcc_arch_flags('11.0', 'Ampere')) ==
       '-gencode arch=compute_80,code=sm_80')

# Splitting & deduplication.
assert(' '.join(cuda.nvcc_arch_flags('11.0', 'Ampere;8.0,8.0')) ==
       '-gencode arch=compute_80,code=sm_80')

# Same, but list supplied as list.
assert(' '.join(cuda.nvcc_arch_flags('11.0', ['Ampere', '8.0', '8.0'])) ==
       '-gencode arch=compute_80,code=sm_80')

# Same, but mode set to Auto with detected set to a string with a variety of separators.
assert(' '.join(cuda.nvcc_arch_flags('11.0', 'Auto', detected: 'Ampere;8.0,8.0')) ==
       '-gencode arch=compute_80,code=sm_80')

# Same, but detected set to a list.
assert(' '.join(cuda.nvcc_arch_flags('11.0', 'Auto', detected: ['Ampere', '8.0', '8.0'])) ==
       '-gencode arch=compute_80,code=sm_80')

# Ask for 8.6 binary with 8.0-level PTX.
assert(' '.join(cuda.nvcc_arch_flags('11.1', '8.6(8.0)')) ==
       '-gencode arch=compute_80,code=sm_86')

# Same, but keep the 8.0 PTX.
assert(' '.join(cuda.nvcc_arch_flags('11.1', '8.6(8.0)+PTX')) ==
       '-gencode arch=compute_80,code=sm_86 -gencode arch=compute_80,code=compute_80')

# Detected Ampere RTX 3090 on CUDA 10.2, saturate to 7.5+PTX
assert(' '.join(cuda.nvcc_arch_flags('10.2', 'Auto', detected: ['8.0'])) ==
       '-gencode arch=compute_75,code=sm_75 -gencode arch=compute_75,code=compute_75')

# Failed to auto-detect with CUDA 10.2, default to common GPUs (3.0;3.5;5.0;5.2;6.0;6.1;7.0;7.5+PTX)
assert(' '.join(cuda.nvcc_arch_flags('10.2', 'Auto', detected: [])) ==
       '-gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=sm_35 '+
       '-gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 '+
       '-gencode arch=compute_60,code=sm_60 -gencode arch=compute_61,code=sm_61 '+
       '-gencode arch=compute_70,code=sm_70 -gencode arch=compute_75,code=sm_75 '+
       '-gencode arch=compute_75,code=compute_75')
