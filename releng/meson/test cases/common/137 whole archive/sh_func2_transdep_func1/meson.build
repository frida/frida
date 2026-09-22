# Same as sh_func2_dep_func1 but dependency is transitive.
# func2.c does not have any reference to func1() so without link_whole compiler
# should throw func1() out.
sh_func2_transdep_func1 = shared_library(
  'sh_func2_transdep_func1', '../func2.c',
  dependencies : func1_trans_dep)
