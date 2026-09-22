#define _XOPEN_SOURCE 500

#include<boost/thread.hpp>

boost::recursive_mutex m;

struct callable {
    void operator()() {
        boost::recursive_mutex::scoped_lock l(m);
    };
};

int main(int argc, char **argv) {
    callable x;
    boost::thread thr(x);
    thr.join();
    return 0;
}
