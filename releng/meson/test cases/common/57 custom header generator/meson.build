project('custom header generator', 'c')

cc_id  = meson.get_compiler('c').get_id()
cc_ver = meson.get_compiler('c').version()

if cc_id == 'intel' or (cc_id == 'lcc' and cc_ver.version_compare('<=1.23.08'))
  # ICC and LCC <= 1.23.08 do not escape spaces in paths in the dependency file, so Ninja
  # (correctly) thinks that the rule has multiple outputs and errors out:
  # 'depfile has multiple output paths'
  error('MESON_SKIP_TEST: Skipping test because your compiler is known to generate broken dependency files')
endif

gen = find_program('makeheader.py')

generated_h = custom_target('makeheader.py',
  output : 'myheader.lh', # Suffix not .h to ensure this works with custom suffixes, too.
  input : 'input.def',
  command : [gen, '@INPUT0@', '@OUTPUT0@', files('somefile.txt')])

prog = executable('prog', 'prog.c', generated_h)
test('gentest', prog)
