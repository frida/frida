# Try using sourceset with various kinds of generated sources

project('a', 'c')

cp = find_program('cp.py')

source_set = import('sourceset')
sources = source_set.source_set()

a_c = custom_target('gen-custom-target',
                    input: 'a.c', output: 'out_a.c',
                    command: [cp, '@INPUT@', '@OUTPUT@'])
sources.add(when: 'YES', if_true: a_c)
sources.add(when: 'YES', if_true: a_c[0])

f_c = configure_file(input: 'f.c', output: 'out_f.c', copy: true)
sources.add(when: 'YES', if_true: f_c)
sources.add(when: 'YES', if_true: f_c)

gen = generator(cp, output: 'out_@PLAINNAME@', arguments: ['@INPUT@', '@OUTPUT@'])
g_c = gen.process(files('g.c'))
sources.add(when: 'YES', if_true: g_c)
sources.add(when: 'YES', if_true: g_c)

conf1 = { 'YES': true, }
result1 = sources.apply(conf1)

executable('first', sources: result1.sources(), dependencies: result1.dependencies())
