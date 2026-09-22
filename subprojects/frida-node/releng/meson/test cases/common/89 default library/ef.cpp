#include"ef.h"

DLL_PUBLIC Ef::Ef() : x(99) {
}

int DLL_PUBLIC Ef::get_x() const {
    return x;
}
