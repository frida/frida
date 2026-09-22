#ifdef CTHING
#error "Wrong local argument set"
#endif

#ifndef CPPTHING
#error "Local argument not set"
#endif

extern "C" int func();

int main(void) {
    return func();
}
