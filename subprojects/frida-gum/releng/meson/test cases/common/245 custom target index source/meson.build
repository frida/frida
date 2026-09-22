project('custom target index source', 'c')

# Test that using a custom target index as a sourcefile works correctly

copy1 = find_program('copyfile.py')
copy2 = find_program('copyfile2.py')

step_1 = custom_target('step_1',
  input: ['code_source.c', files('header_source.h')],
  output: ['step_1.c', 'step_1.h'],
  command: [copy2, '@INPUT0@', '@OUTPUT0@', '@INPUT1@', '@OUTPUT1@'],
  build_by_default: false)

# test custom target with a single CustomTargetIndex input
step_2_c = custom_target('step_2_c',
  input: step_1[0],
  output: 'step_2.c',
  command: [copy1, '@INPUT0@', '@OUTPUT0@'],
  build_by_default: false)

step_2_h = custom_target('step_2_h',
  input: step_1[1],
  output: 'step_2.h',
  command: [copy1, '@INPUT0@', '@OUTPUT0@'],
  build_by_default: false,
)

# test custom target with multiple CustomTargetIndex inputs
gen = custom_target('step_3',
  input: [step_2_c, step_2_h],
  output: ['gen.c', 'gen.h'],
  command: [copy2, '@INPUT0@', '@OUTPUT0@', '@INPUT1@', '@OUTPUT1@'],
  build_by_default: false)
gen_c = gen[0]
gen_h = gen[1]

exe_separate = executable('exe_separate',
  ['main.c', gen_c, gen_h],
  build_by_default: false,
  install: false,
)

exe_together = executable('exe_together',
  ['main.c', gen],
  build_by_default: false,
  install: false,
)

# also cover passing custom target to tests as arguments
test('exe_separate', exe_separate, args: [gen_c, gen_h])
test('exe_together', exe_together, args: gen)
