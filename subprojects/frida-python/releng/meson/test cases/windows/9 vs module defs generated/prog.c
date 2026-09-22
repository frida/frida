int somedllfunc(void);

int exefunc(void) {
    return 42;
}

int main(void) {
    return somedllfunc() == exefunc() ? 0 : 1;
}
