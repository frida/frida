# Same as sh_func2_linked_func1, # func2.c does not depend on func1(),
# so without link_whole compiler would throw func1() away.
# This is the same version of the test with a dependency object instead.
sh_func2_dep_func1 = shared_library('sh_func2_dep_func1', '../func2.c', dependencies : func1_dep)
