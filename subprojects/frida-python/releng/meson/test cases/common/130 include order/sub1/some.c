#if defined _WIN32 || defined __CYGWIN__
  __declspec(dllexport)
#endif
int somefunc(void) {
  return 1984;
}
