internal import FridaCore

extension GLib {
    public final class File: @unchecked Sendable, CustomStringConvertible, Equatable, Hashable {
        internal let handle: OpaquePointer

        public convenience init(file: String) throws {
            Runtime.ensureInitialized()

            self.init(handle: g_file_new_for_path(file)!)
        }

        public init(handle: OpaquePointer) {
            self.handle = handle
        }

        deinit {
            g_object_unref(gpointer(handle))
        }

        public var description: String {
            "GLib.File()"
        }

        public static func == (lhs: File, rhs: File) -> Bool {
            lhs.handle == rhs.handle
        }

        public func hash(into hasher: inout Hasher) {
            hasher.combine(UInt(bitPattern: handle))
        }

        public var path: String? {
            guard let raw = g_file_get_path(handle) else { return nil }
            return String(cString: raw)
        }
    }
}
