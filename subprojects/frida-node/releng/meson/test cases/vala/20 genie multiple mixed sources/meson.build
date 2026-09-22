project( 'Genie multiple and mixed sources', 'vala', 'c' )

genie_deps = [
    dependency( 'glib-2.0' ),
    dependency( 'gobject-2.0' ),
]

sources = [
    'init.gs',
    'test_one.gs',
    'test_two.gs',
    'vala_test_one.vala',
    'vala_test_two.vala',
    'c_test_one.c',
    'c_test_two.c',
]

prog = executable( 'genie_prog', sources, dependencies: genie_deps )
test( 'Given a Genie program when it is compiled from multiple mixed sources then it should work', prog )
