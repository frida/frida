project('D Features', 'd', default_options : ['debug=false'])

dc = meson.get_compiler('d')

# GDC 13 hard errors if options are given number values.
# https://github.com/mesonbuild/meson/pull/11996

if dc.get_id() == 'gcc' and dc.version().version_compare('>=13')
    number_options_supported = false
else
    number_options_supported = true
endif

# ONLY FOR BACKWARDS COMPATIBILITY.
# DO NOT DO THIS IN NEW CODE!
# USE include_directories() INSTEAD OF BUILDING
# STRINGS TO PATHS MANUALLY!
data_dir = join_paths(meson.current_source_dir(), 'data')

test_src = ['app.d', 'extra.d']

e_plain_bcompat = executable('dapp_menu_bcompat',
    test_src,
    d_import_dirs: [data_dir]
)
test('dapp_menu_t_fail_bcompat', e_plain_bcompat, should_fail: true)
test('dapp_menu_t_bcompat', e_plain_bcompat, args: ['menu'])

# directory for data
# This is the correct way to do this.
data_dir = include_directories('data')

e_plain = executable('dapp_menu',
    test_src,
    d_import_dirs: [data_dir]
)
test('dapp_menu_t_fail', e_plain, should_fail: true)
test('dapp_menu_t', e_plain, args: ['menu'])


# test feature versions and string imports
e_versions = executable('dapp_versions',
    test_src,
    d_import_dirs: [data_dir],
    d_module_versions: ['No_Menu', 'With_People']
)
test('dapp_versions_t_fail', e_versions, args: ['menu'], should_fail: true)
test('dapp_versions_t', e_versions, args: ['people'])

# test everything and unittests
e_test = executable('dapp_test',
    test_src,
    d_import_dirs: [data_dir],
    d_module_versions: ['No_Menu', 'With_People'],
    d_unittest: true
)
test('dapp_test', e_test)

# test version level
if number_options_supported
    e_version_int = executable('dapp_version_int',
        test_src,
        d_import_dirs: [data_dir],
       d_module_versions: ['With_VersionInteger', 3],
    )
    test('dapp_version_int_t', e_version_int, args: ['debug'])

# test version level failure
e_version_int_fail = executable('dapp_version_int_fail',
    test_src,
    d_import_dirs: [data_dir],
    d_module_versions: ['With_VersionInteger', 2],
)
test('dapp_version_int_t_fail', e_version_int_fail, args: ['debug'], should_fail: true)
endif

# test debug conditions: disabled
e_no_debug = executable('dapp_no_debug',
    test_src,
    d_import_dirs: [data_dir],
    d_module_versions: ['With_Debug'],
)
test('dapp_no_debug_t_fail', e_no_debug, args: ['debug'], should_fail: true)

if number_options_supported
    # test debug conditions: enabled
    e_debug = executable('dapp_debug',
        test_src,
        d_import_dirs: [data_dir],
        d_module_versions: ['With_Debug'],
        d_debug: 1,
    )
    test('dapp_debug_t', e_debug, args: ['debug'])

    # test debug conditions: integer
    e_debug_int = executable('dapp_debug_int',
        test_src,
        d_import_dirs: [data_dir],
        d_module_versions: ['With_DebugInteger'],
        d_debug: 3,
    )
    test('dapp_debug_int_t', e_debug_int, args: ['debug'])

    # test with all debug conditions at once, and with redundant values
    e_debug_all = executable('dapp_debug_all',
        test_src,
        d_import_dirs: [data_dir],
        d_module_versions: ['With_DebugAll'],
        d_debug: ['4', 'DebugIdentifier', 2, 'DebugIdentifierUnused'],
    )
    test('dapp_debug_all_t', e_debug_all, args: ['debug'])
endif

# test debug conditions: identifier
e_debug_ident = executable('dapp_debug_ident',
    test_src,
    d_import_dirs: [data_dir],
    d_module_versions: ['With_DebugIdentifier'],
    d_debug: 'DebugIdentifier',
)
test('dapp_debug_ident_t', e_debug_ident, args: ['debug'])
