static int duplicate_func(void) {
    return 4;
}

int func(void) {
    return duplicate_func();
}
