#if SOME_MAGIC_DEFINE != 42
#error "SOME_MAGIC_DEFINE != 42"
#endif

int foo(int x) {
  return 42 - x;
}
