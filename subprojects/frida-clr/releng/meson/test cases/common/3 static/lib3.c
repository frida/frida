int func3(const int x) {
    return x + 1;
}

#ifndef WORK
# error "did not get static only C args"
#endif

#ifdef BREAK
# error "got shared only C args, but shouldn't have"
#endif
