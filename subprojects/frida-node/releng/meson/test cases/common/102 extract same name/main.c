int func1(void);
int func2(void);

int main(void) {
    return !(func1() == 23 && func2() == 42);
}
