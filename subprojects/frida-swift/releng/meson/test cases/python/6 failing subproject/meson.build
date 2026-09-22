project('foo', 'cpp')

# Regression test for https://github.com/mesonbuild/meson/issues/9038
dependency('bar', required: false, allow_fallback: true)
python = import('python').find_installation('python3').dependency()
