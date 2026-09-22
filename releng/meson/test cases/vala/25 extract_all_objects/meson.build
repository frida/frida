project('valatest', 'vala', 'c')

valadeps = [dependency('glib-2.0'), dependency('gobject-2.0')]

# A dummy target, just so that we can check that extract_all_objects() works
# properly for Vala sources.  Due to a bug, Vala's .c.o files weren't included
# which would cause the "e" executable below to have no "main" and fail to link.
dummy = executable('valaprog_dummy', 'prog.vala', dependencies : valadeps, build_by_default: false)

objs = dummy.extract_all_objects()
e = executable('valaprog', objects: objs, dependencies : valadeps)
