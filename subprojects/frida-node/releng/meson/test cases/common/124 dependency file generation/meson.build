project('dep file gen', 'c')

cc_id  = meson.get_compiler('c').get_id()
cc_ver = meson.get_compiler('c').version()

if cc_id == 'intel' or (cc_id == 'lcc' and cc_ver.version_compare('<=1.23.08'))
  # ICC and LCC <= 1.23.08 do not escape spaces in paths in the dependency file, so Ninja
  # (correctly) thinks that the rule has multiple outputs and errors out:
  # 'depfile has multiple output paths'
  error('MESON_SKIP_TEST: Skipping test because your compiler is known to generate broken dependency files')
endif

e = executable('main file', 'main .c')
test('test it', e)
