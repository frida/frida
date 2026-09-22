int func1(void);
int func2(void);

int static_lib_func(void) {
    return func1() + func2();
}
