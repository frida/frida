class MainProg : GLib.Object {

    public static int main(string[] args) {
        stdout.printf("DATA_DIRECTORY is: %s.\n", DATA_DIRECTORY);
        stdout.printf("SOMETHING_ELSE is: %s.\n", SOMETHING_ELSE);
        return 0;
    }
}
