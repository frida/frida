project('object extraction', 'c')

if meson.backend() == 'xcode'
    # Xcode gives object files unique names but only if they would clash. For example
    # two files named lib.o instead get the following names:
    #
    # lib-4fbe522d8ba4cb1f1b89cc2df640a2336b92e1a5565f0a4c5a79b5b5e2969eb9.o
    # lib-4fbe522d8ba4cb1f1b89cc2df640a2336deeff2bc2297affaadbe20f5cbfee56.o
    #
    # No-one has reverse engineered the naming scheme so we would access them.
    # IF you feel up to the challenge, patches welcome.
    error('MESON_SKIP_TEST, Xcode cannot extract objs when they would have the same filename.')
endif

lib = library('somelib', ['lib.c', 'src/lib.c'])
# Also tests that the object list is flattened properly
obj = lib.extract_objects(['lib.c', ['src/lib.c']])
exe = executable('main', 'main.c', objects: obj)
test('extraction', exe)
