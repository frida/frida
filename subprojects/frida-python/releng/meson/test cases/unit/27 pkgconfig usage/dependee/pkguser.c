#include<pkgdep.h>

int main(int argc, char **argv) {
    int res = pkgdep();
    return res != 99;
}
