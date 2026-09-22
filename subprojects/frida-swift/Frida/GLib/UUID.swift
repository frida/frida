internal import FridaCore

extension GLib {
    public enum UUID {
        public static func stringRandom() -> String {
            let cstr = g_uuid_string_random()!
            let result = String(cString: cstr)
            g_free(cstr)
            return result
        }
    }
}
