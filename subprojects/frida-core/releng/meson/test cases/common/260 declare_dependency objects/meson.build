# Test that declare_dependency(objects: ...) fixes issues with duplicated
# objects in the final link line, thanks to deduplication of dependencies.
# The commented declare_dependency() invocation using link_whole instead
# fails thusly:
#
#     ar csrDT libbar.a libfoo.a.p/foo.c.o libbar.a.p/bar.c.o
#     ar csrDT libfoo.a libfoo.a.p/foo.c.o
#     cc  -o prog prog.p/prog.c.o -Wl,--as-needed -Wl,--no-undefined -Wl,--whole-archive -Wl,--start-group libfoo.a libbar.a -Wl,--end-group -Wl,--no-whole-archive
#     /usr/bin/ld: libfoo.a.p/foo.c.o: in function `foo':
#     ../foo.c:3: multiple definition of `foo'; libfoo.a.p/foo.c.o:../foo.c:3: first defined here

project('Transitive declare_dependency(objects)', 'c')

libfoo = static_library('foo', 'foo.c')
#foo = declare_dependency(link_whole: libfoo)
foo = declare_dependency(objects: libfoo.extract_all_objects(recursive: true))

libbar = static_library('bar', 'bar.c', dependencies: foo)

#bar = declare_dependency(link_whole: libbar, dependencies: foo)
bar = declare_dependency(objects: libbar.extract_all_objects(recursive: true), dependencies: foo)

executable('prog', sources: files('prog.c'), dependencies: [foo, bar])
