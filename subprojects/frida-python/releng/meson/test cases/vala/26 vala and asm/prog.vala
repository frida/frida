extern int get_retval();

class MainProg : GLib.Object {

    public static int main(string[] args) {
        stdout.printf("Vala is working.\n");
        return get_retval();
    }
}
