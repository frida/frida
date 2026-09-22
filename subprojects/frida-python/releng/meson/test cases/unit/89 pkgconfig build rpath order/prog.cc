#include <string>
#include <iostream>

int main(int argc, char **argv) {
    std::string* s = new std::string("Hello");
    delete s;
    return 0;
}
