    public convenience init(path: String) {
        Runtime.ensureInitialized()
        self.init(handle: frida_file_monitor_new(path))
    }
