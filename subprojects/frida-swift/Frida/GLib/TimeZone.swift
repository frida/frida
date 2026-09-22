internal import FridaCore

extension GLib {
    public final class TimeZone: CustomStringConvertible, Equatable, Hashable {
        internal let handle: OpaquePointer

        public static func utc() -> TimeZone {
            return TimeZone(handle: g_time_zone_new_utc())
        }

        public convenience init(identifier: String) throws {
            guard let tz = g_time_zone_new_identifier(identifier) else {
                throw Error.invalidArgument("Invalid timezone identifier: \(identifier)")
            }
            self.init(handle: tz)
        }

        public init(handle: OpaquePointer) {
            self.handle = handle
        }

        deinit {
            g_time_zone_unref(handle)
        }

        public var description: String {
            "GLib.TimeZone(\(String(cString: g_time_zone_get_identifier(handle)!)))"
        }

        public static func == (lhs: TimeZone, rhs: TimeZone) -> Bool {
            lhs.handle == rhs.handle
        }

        public func hash(into hasher: inout Hasher) {
            hasher.combine(UInt(bitPattern: handle))
        }
    }
}
