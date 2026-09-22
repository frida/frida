#include <stdio.h>

int meson_test_main_foo(void);
int meson_test_subproj_foo(void);

int main(void) {
    if (meson_test_main_foo() != 10) {
        printf("Failed meson_test_main_foo\n");
        return 1;
    }
    if (meson_test_subproj_foo() != 20) {
        printf("Failed meson_test_subproj_foo\n");
        return 1;
    }
    return 0;
}
