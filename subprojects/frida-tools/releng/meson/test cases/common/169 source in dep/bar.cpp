extern "C" int foo(void);

int main(void) {
    return foo() != 42;
}
