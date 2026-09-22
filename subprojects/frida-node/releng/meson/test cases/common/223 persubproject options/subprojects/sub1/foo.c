int foo(void);

int foo(void) {
  /* This is built with -Werror, it would error if warning_level=3 was inherited
   * from main project and not overridden by this subproject's default_options. */
  int x;
  return 0;
}
