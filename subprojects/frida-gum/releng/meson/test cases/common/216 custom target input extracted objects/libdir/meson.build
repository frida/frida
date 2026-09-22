gen_py = find_program('gen.py')
ctsrc = custom_target('custom_target sources',
                       output: 'ct-source.c',
                       input: 'source.c',
                       command: [ gen_py, '@INPUT@', 'func1', 'func2' ], capture: true)

gen = generator(gen_py, arguments: ['@INPUT@', 'func1', 'func3'],
                output: 'gen-@PLAINNAME@',
                capture: true)
gensrc = gen.process('source.c')


gen = generator(gen_py, arguments: ['@INPUT@', 'func1', 'func4'],
                output: 'gen-@PLAINNAME@',
                capture: true)
sublibsrc = gen.process('source.c')
subobjlib = static_library('subobject', sublibsrc)

objlib = static_library('object', 'source.c', ctsrc, gensrc,
                        objects: subobjlib.extract_all_objects(recursive: false),
                        override_options : ['unity=off'])
