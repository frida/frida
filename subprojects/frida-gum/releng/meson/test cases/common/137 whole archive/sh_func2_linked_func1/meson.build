# Nothing in func2.c uses func1, so the linker would throw it
# away and thus linking the exe would fail.
sh_func2_linked_func1 = shared_library('sh_func2_linked_func1', '../func2.c', link_whole : st_func1)
