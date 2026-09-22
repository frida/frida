extern void * get_ret_code ();

public class MyThread : Object {
    public int x_times { get; private set; }

    public MyThread (int times) {
        this.x_times = times;
    }

    public int run () {
        for (int i = 0; i < this.x_times; i++) {
            stdout.printf ("ping! %d/%d\n", i + 1, this.x_times);
            Thread.usleep (10000);
        }

        // return & exit have the same effect
        Thread.exit (get_ret_code ());
        return 43;
    }
}

public static int main (string[] args) {
    // Check whether threads are supported:
    if (Thread.supported () == false) {
        stderr.printf ("Threads are not supported!\n");
        return -1;
    }

    try {
        // Start a thread:
        MyThread my_thread = new MyThread (10);
        Thread<int> thread = new Thread<int>.try ("My fst. thread", my_thread.run);

        // Wait until thread finishes:
        int result = thread.join ();
        // Output: `Thread stopped! Return value: 42`
        stdout.printf ("Thread stopped! Return value: %d\n", result);
    } catch (Error e) {
        stdout.printf ("Error: %s\n", e.message);
    }

    return 0;
}
