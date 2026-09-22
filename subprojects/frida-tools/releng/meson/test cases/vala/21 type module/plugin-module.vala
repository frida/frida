public class Foo.PluginModule : TypeModule {

    [CCode (has_target = false)]
    private delegate Type PluginInit (TypeModule type_module);

    public string? directory { get; construct; default = null; }

    public string name { get; construct; }

    public string path { get; construct; }

    public Type plugin_type { get; private set; }

    private Module? module = null;

    public PluginModule (string? directory, string name) {
        Object (directory: directory, name: name);
    }

    construct {
        path = Module.build_path (directory, name);
    }

    public override bool load () {
        module = Module.open (path, ModuleFlags.BIND_LAZY);

        if (module == null) {
            critical (Module.error ());
            return false;
        }

        void* plugin_init;
        if (!module.symbol ("plugin_init", out plugin_init)){
            critical (Module.error ());
            return false;
        }

        if (plugin_init == null) {
            return false;
        }

        plugin_type = ((PluginInit) plugin_init) (this);

        if (!plugin_type.is_a (typeof (Plugin))) {
            return false;
        }

        return true;
    }

    public override void unload () {
        module = null;
    }
}
