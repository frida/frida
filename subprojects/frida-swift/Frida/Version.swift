internal import FridaCore

public var fridaVersion: String {
    String(cString: frida_version_string())
}
