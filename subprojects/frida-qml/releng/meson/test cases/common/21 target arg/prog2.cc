#ifdef CTHING
#error "Local C argument set in wrong target"
#endif

#ifdef CPPTHING
#error "Local CPP argument set in wrong target"
#endif

extern "C" int func();

int main(void) {
    return func();
}
