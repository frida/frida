project('option override', 'c',
  default_options : 'unity=on')

executable('mustunity', 'one.c', 'two.c')
executable('notunity', 'three.c', 'four.c',
  override_options : ['unity=off'])
