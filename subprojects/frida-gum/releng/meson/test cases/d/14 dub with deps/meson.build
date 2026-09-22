project('dub-with-deps-example', ['d'])

error('MESON_SKIP_TEST: Dub support is broken at the moment (#11773)')

dub_exe = find_program('dub', required : false)
if not dub_exe.found()
    error('MESON_SKIP_TEST: Dub not found')
endif

if meson.get_compiler('d').get_id() == 'gcc'
    error('MESON_SKIP_TEST: can\'t build dependencies with GDC')
elif meson.get_compiler('d').get_id() == 'llvm'
    dc = 'ldc2'
elif meson.get_compiler('d').get_id() == 'dmd'
    dc = 'dmd'
endif

arch = host_machine.cpu_family()

if host_machine.system() == 'windows'
    # check if toolchain is 32bits
    sz = meson.get_compiler('d').sizeof('void*')
    if arch == 'x86' or sz == 4
        arch = 'x86_mscoff'
    endif
endif

run_command('dub', 'run', 'dub-build-deep', '--yes', '--', 'xlsx', '--compiler', dc, '--arch', arch,
    check: true,
)

xlsx_dep = dependency('xlsx', method: 'dub')

test_exe = executable('test-test6', 'test.d', dependencies: xlsx_dep)

test('test dub with deps', test_exe)
