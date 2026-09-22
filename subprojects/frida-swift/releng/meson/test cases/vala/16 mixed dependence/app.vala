namespace App {
    public static int main(string[] args) {
        var mixer = new Mixer();
        print("Current volume is %u\n", mixer.get_volume());
        return 0;
    }
}
