project('cppmodules', 'cpp', default_options: ['cpp_std=c++latest'])

cpp = meson.get_compiler('cpp')

if cpp.get_id() == 'msvc'
    subdir('vs')
elif cpp.get_id() == 'gcc'
    subdir('gcc')
else
    error('Unknown compiler')
endif
