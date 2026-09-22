int func(void);

static int duplicate_func(void) {
    return -4;
}

int main(void) {
    return duplicate_func() + func();
}
