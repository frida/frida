project('meson-dep-test', 'd')

generator = executable('generator', 'generator.d')

generated = custom_target('generated',
    capture: true,
    output: 'generated.d',
    input: 'input.txt',
    command: [
        generator, '@INPUT@'
    ]
)

exe = executable('exe', generated, 'exe.d',
    include_directories: include_directories('.'),
)

test('test exe', exe)
