extern "C" int cfunc();

void func(void) {
    std::cout << "This is a function that fails to compile if iostream is not included."
              << std::endl;
}

int main(void) {
    return cfunc();
}
