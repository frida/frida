project('test', 'c')

libxserver_dri3 = []
libxserver = [ libxserver_dri3 ]

executable('Xephyr', 'xephyr.c', link_with: [ libxserver ])

executable('Zephyr', 'xephyr.c', link_args: [[], []])
