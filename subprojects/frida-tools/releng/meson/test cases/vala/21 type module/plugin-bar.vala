[ModuleInit]
public GLib.Type plugin_init (GLib.TypeModule tm) {
    return typeof (Bar.Plugin);
}

public class Bar.Plugin : Foo.Plugin, GLib.Object {

    public string bar () {
        return "bar";
    }
}
