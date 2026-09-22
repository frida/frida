project('rust and c polyglot executable', 'c', 'rust')

if host_machine.system() == 'darwin'
  error('MESON_SKIP_TEST: does not work right on macos, please fix!')
endif

cc = meson.get_compiler('c')

# Test all combinations of crate and target types.
# - 'clib' gets translated to `rust_abi: 'c'` instead.
# - '' gets translated to no kwargs.
allowed_map = {
  'static_library': ['rlib', 'staticlib', 'lib', 'clib', ''],
  'shared_library': ['dylib', 'cdylib', 'lib', 'proc-macro', 'clib', ''],
  'both_libraries': ['lib', 'clib', ''],
}
foreach crate_type : ['lib', 'rlib', 'dylib', 'cdylib', 'staticlib', 'proc-macro', 'clib', '', 'invalid']
  foreach target_type, allowed : allowed_map
    name = f'stuff_@crate_type@_@target_type@'.underscorify()
    src = crate_type == 'proc-macro' ? 'proc.rs' : 'stuff.rs'
    if crate_type not in allowed
      # Note: in the both_libraries() case it is possible that the static part
      # is still being built because the shared part raised an error but we
      # don't rollback correctly.
      testcase expect_error('(Crate type .* invalid for .*)|(.*must be one of.*not invalid)', how: 're')
        build_target(name, src,
          target_type: target_type,
          rust_crate_type: crate_type,
          install: true)
      endtestcase
      continue
    endif
    rust_kwargs = {}
    if crate_type == 'clib'
      rust_kwargs = {'rust_abi': 'c'}
    elif crate_type != ''
      rust_kwargs = {'rust_crate_type': crate_type}
    endif
    l = build_target(name, src,
      target_type: target_type,
      kwargs: rust_kwargs,
      install: true)
    if crate_type in ['cdylib', 'staticlib', 'clib']
      e = executable(f'prog-@name@', 'prog.c',
        link_with: l,
        rust_dependency_map: {name: 'stuff'},
        install: true)
      test(f'polyglottest-@name@', e)
    else
      testcase expect_error('Try to link Rust ABI library .*', how: 're')
        executable(f'prog-@name@', 'prog.c', link_with: l)
      endtestcase
    endif
  endforeach
endforeach
