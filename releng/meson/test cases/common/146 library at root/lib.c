#if defined _WIN32 || defined __CYGWIN__
__declspec(dllexport)
#endif
int fn(void) {
    return -1;
}
