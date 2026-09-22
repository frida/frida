gen = executable('gen', 'main.swift')

srcs = custom_target('gensrc',
  output : 'gen.swift',
  command : [gen, '@OUTPUT@']
)
