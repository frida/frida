int foo(void);

#ifdef __GNUC__
#warning This should not produce error
#endif

int foo(void) {
  return 0;
}
