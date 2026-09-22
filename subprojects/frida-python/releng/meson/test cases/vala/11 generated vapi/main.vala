using Foo;
using Bar;

class Main : GLib.Object {
    public static int main(string[] args) {
        var ignore = Foo.Foo.return_success();
        return Bar.Bar.return_success();
    }
}
