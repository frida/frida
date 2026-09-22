---
short-description: CUDA module
authors:
    - name: Olexa Bilaniuk
      years: [2019]
      has-copyright: false
...

# Unstable CUDA Module
_Since: 0.50.0_

This module provides helper functionality related to the CUDA Toolkit and
building code using it.


**Note**: this module is unstable. It is only provided as a technology preview.
Its API may change in arbitrary ways between releases or it might be removed
from Meson altogether.


## Importing the module

The module may be imported as follows:

``` meson
cuda = [[#import]]('unstable-cuda')
```

It offers several useful functions that are enumerated below.


## Functions

### `nvcc_arch_flags()`
_Since: 0.50.0_

``` meson
cuda.nvcc_arch_flags(cuda_version_string, ...,
                     detected: string_or_array)
```

Returns a list of `-gencode` flags that should be passed to `cuda_args:` in
order to compile a "fat binary" for the architectures/compute capabilities
enumerated in the positional argument(s). The flags shall be acceptable to
an NVCC with CUDA Toolkit version string `cuda_version_string`.

A set of architectures and/or compute capabilities may be specified by:

- The single positional argument `'All'`, `'Common'` or `'Auto'`
- As (an array of)
  - Architecture names (`'Kepler'`, `'Maxwell+Tegra'`, `'Turing'`) and/or
  - Compute capabilities (`'3.0'`, `'3.5'`, `'5.3'`, `'7.5'`)

A suffix of `+PTX` requests PTX code generation for the given architecture.
A compute capability given as `A.B(X.Y)` requests PTX generation for an older
virtual architecture `X.Y` before binary generation for a newer architecture
`A.B`.

Multiple architectures and compute capabilities may be passed in using

- Multiple positional arguments
- Lists of strings
- Space (` `), comma (`,`) or semicolon (`;`)-separated strings

The single-word architectural sets `'All'`, `'Common'` or `'Auto'`
cannot be mixed with architecture names or compute capabilities. Their
interpretation is:

| Name              | Compute Capability |
|-------------------|--------------------|
| `'All'`           | All CCs supported by given NVCC compiler. |
| `'Common'`        | Relatively common CCs supported by given NVCC compiler. Generally excludes Tegra and Tesla devices. |
| `'Auto'`          | The CCs provided by the `detected:` keyword, filtered for support by given NVCC compiler. |

The supported architecture names and their corresponding compute capabilities
are:

| Name              | Compute Capability |
|-------------------|--------------------|
| `'Fermi'`         | 2.0, 2.1(2.0)      |
| `'Kepler'`        | 3.0, 3.5           |
| `'Kepler+Tegra'`  | 3.2                |
| `'Kepler+Tesla'`  | 3.7                |
| `'Maxwell'`       | 5.0, 5.2           |
| `'Maxwell+Tegra'` | 5.3                |
| `'Pascal'`        | 6.0, 6.1           |
| `'Pascal+Tegra'`  | 6.2                |
| `'Volta'`         | 7.0                |
| `'Xavier'`        | 7.2                |
| `'Turing'`        | 7.5                |
| `'Ampere'`        | 8.0, 8.6           |


Examples:

    cuda.nvcc_arch_flags('10.0', '3.0', '3.5', '5.0+PTX')
    cuda.nvcc_arch_flags('10.0', ['3.0', '3.5', '5.0+PTX'])
    cuda.nvcc_arch_flags('10.0', [['3.0', '3.5'], '5.0+PTX'])
    cuda.nvcc_arch_flags('10.0', '3.0 3.5 5.0+PTX')
    cuda.nvcc_arch_flags('10.0', '3.0,3.5,5.0+PTX')
    cuda.nvcc_arch_flags('10.0', '3.0;3.5;5.0+PTX')
    cuda.nvcc_arch_flags('10.0', 'Kepler 5.0+PTX')
    # Returns ['-gencode', 'arch=compute_30,code=sm_30',
    #          '-gencode', 'arch=compute_35,code=sm_35',
    #          '-gencode', 'arch=compute_50,code=sm_50',
    #          '-gencode', 'arch=compute_50,code=compute_50']

    cuda.nvcc_arch_flags('10.0', '3.5(3.0)')
    # Returns ['-gencode', 'arch=compute_30,code=sm_35']

    cuda.nvcc_arch_flags('8.0', 'Common')
    # Returns ['-gencode', 'arch=compute_30,code=sm_30',
    #          '-gencode', 'arch=compute_35,code=sm_35',
    #          '-gencode', 'arch=compute_50,code=sm_50',
    #          '-gencode', 'arch=compute_52,code=sm_52',
    #          '-gencode', 'arch=compute_60,code=sm_60',
    #          '-gencode', 'arch=compute_61,code=sm_61',
    #          '-gencode', 'arch=compute_61,code=compute_61']

    cuda.nvcc_arch_flags('9.2', 'Auto', detected: '6.0 6.0 6.0 6.0')
    cuda.nvcc_arch_flags('9.2', 'Auto', detected: ['6.0', '6.0', '6.0', '6.0'])
    # Returns ['-gencode', 'arch=compute_60,code=sm_60']

    cuda.nvcc_arch_flags(nvcc, 'All')
    # Returns ['-gencode', 'arch=compute_20,code=sm_20',
    #          '-gencode', 'arch=compute_20,code=sm_21',
    #          '-gencode', 'arch=compute_30,code=sm_30',
    #          '-gencode', 'arch=compute_32,code=sm_32',
    #          '-gencode', 'arch=compute_35,code=sm_35',
    #          '-gencode', 'arch=compute_37,code=sm_37',
    #          '-gencode', 'arch=compute_50,code=sm_50', # nvcc.version()  <  7.0
    #          '-gencode', 'arch=compute_52,code=sm_52',
    #          '-gencode', 'arch=compute_53,code=sm_53', # nvcc.version() >=  7.0
    #          '-gencode', 'arch=compute_60,code=sm_60',
    #          '-gencode', 'arch=compute_61,code=sm_61', # nvcc.version() >=  8.0
    #          '-gencode', 'arch=compute_70,code=sm_70',
    #          '-gencode', 'arch=compute_72,code=sm_72', # nvcc.version() >=  9.0
    #          '-gencode', 'arch=compute_75,code=sm_75'] # nvcc.version() >= 10.0

_Note:_ This function is intended to closely replicate CMake's FindCUDA module
function `CUDA_SELECT_NVCC_ARCH_FLAGS(out_variable, [list of CUDA compute architectures])`



### `nvcc_arch_readable()`
_Since: 0.50.0_

``` meson
cuda.nvcc_arch_readable(cuda_version_string, ...,
                        detected: string_or_array)
```

Has precisely the same interface as [`nvcc_arch_flags()`](#nvcc_arch_flags),
but rather than returning a list of flags, it returns a "readable" list of
architectures that will be compiled for. The output of this function is solely
intended for informative message printing.

    archs    = '3.0 3.5 5.0+PTX'
    readable = cuda.nvcc_arch_readable('10.0', archs)
    message('Building for architectures ' + ' '.join(readable))

This will print

    Message: Building for architectures sm30 sm35 sm50 compute50

_Note:_ This function is intended to closely replicate CMake's
FindCUDA module function `CUDA_SELECT_NVCC_ARCH_FLAGS(out_variable,
[list of CUDA compute architectures])`



### `min_driver_version()`
_Since: 0.50.0_

``` meson
cuda.min_driver_version(cuda_version_string)
```

Returns the minimum NVIDIA proprietary driver version required, on the
host system, by kernels compiled with a CUDA Toolkit with the given
version string.

The output of this function is generally intended for informative
message printing, but could be used for assertions or to conditionally
enable features known to exist within the minimum NVIDIA driver
required.
