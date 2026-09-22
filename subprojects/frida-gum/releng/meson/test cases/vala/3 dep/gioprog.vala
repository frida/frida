class GioProg {

    public static int main(string[] args) {
        var homedir = File.new_for_path(Environment.get_home_dir());
        stdout.printf("Home directory as told by gio is " + homedir.get_path() + "\n");
        return 0;
    }
}
