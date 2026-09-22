project('dub-example', 'd')

error('MESON_SKIP_TEST: Dub support is broken at the moment (#11773)')

dub_exe = find_program('dub', required : false)
if not dub_exe.found()
  error('MESON_SKIP_TEST: Dub not found')
endif

urld_dep = dependency('urld', method: 'dub')

test_exe = executable('test-urld', 'test.d', dependencies: urld_dep)
test('test urld', test_exe)

# If you want meson to generate/update a dub.json file
dlang = import('dlang')
dlang.generate_dub_file(meson.project_name().to_lower(), meson.source_root(),
                        authors: 'Meson Team',
                        description: 'Test executable',
                        copyright: 'Copyright © 2018, Meson Team',
                        license: 'MIT',
                        sourceFiles: 'test.d',
                        targetType: 'executable',
                        dependencies: urld_dep
)