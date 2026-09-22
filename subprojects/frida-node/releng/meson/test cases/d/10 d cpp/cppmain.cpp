extern "C" int rt_init();
extern "C" int rt_term();
extern void print_hello(int i);

int main(int, char**) {
    // initialize D runtime
    if (!rt_init())
        return 1;

    print_hello(1);

    // terminate D runtime, each initialize call
    // must be paired with a terminate call.
    if (!rt_term())
        return 1;

    return 0;
}
