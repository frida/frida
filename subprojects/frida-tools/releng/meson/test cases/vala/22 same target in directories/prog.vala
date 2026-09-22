int main() {
    var test1 = new Test ();
    var test2 = new Subdir.Test ();
    var test3 = new Subdir2.Test ();
    var test4 = new Subdir.Subdir2.Test ();
    stdout.printf("Vala is working.\n");
    return 0;
}
