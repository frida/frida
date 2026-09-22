# Regression test for a diamond dependency graph:
#          ┌►R1┐
# main-►R3─┤   ├─►C1
#          └►R2┘
# Both libr1.rlib and libr2.rlib used to contain func.c.o. That was causing
# libr3.rlib to have duplicated func.c.o and then libmain.so failed to link:
# multiple definition of `c_func'.

libc1 = static_library('c1', 'func.c')
libr1 = static_library('r1', 'r1.rs', link_with: libc1)
libr2 = static_library('r2', 'r2.rs', link_with: libc1)
libr3 = static_library('r3', 'r3.rs',
  link_with: [libr1, libr2],
  rust_abi: 'c',
)
shared_library('main', 'main.c', link_whole: [libr3])

# Same dependency graph, but r3 is now installed. Since c1, r1 and r2 are
# not installed, r3 must contain them.
libr3 = static_library('r3-installed', 'r3.rs',
  link_with: [libr1, libr2],
  rust_abi: 'c',
  install: true,
)
shared_library('main-installed', 'main.c', link_with: [libr3])
