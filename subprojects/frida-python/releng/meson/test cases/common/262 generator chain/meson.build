project('Generator Chain', 'c')

stage1_exe = find_program('stage1.py')

stage2_exe = find_program('stage2.py')

stage1_gen = generator(stage1_exe,
  output : '@PLAINNAME@.inter',
  arguments : ['@INPUT@', '@OUTPUT@'])

stage2_gen = generator(stage2_exe,
  output : '@PLAINNAME@.c',
  arguments : ['@INPUT@', '@OUTPUT@'])

out = stage2_gen.process(stage1_gen.process('data.txt'))

hello = executable('hello', out)

test('basic', hello)
