project('extract nonexisting gen', 'c')

lib1 = library('lib1', 'src1.c', 'src2.c', override_options: ['unity=on'])
lib2 = library('lib2', objects: lib1.extract_objects('src1.c'))
