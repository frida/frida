internal import FridaCore

extension GLib {
    public final class DateTime: CustomStringConvertible, Equatable, Hashable {
        internal let handle: OpaquePointer

        public static func nowUTC() -> DateTime {
            return DateTime(handle: g_date_time_new_now_utc())
        }

        public convenience init(
            iso8601 text: String,
            defaultTimeZone: GLib.TimeZone? = nil
        ) throws {
            guard let dt = g_date_time_new_from_iso8601(text, defaultTimeZone?.handle) else {
                throw Error.invalidArgument("Invalid ISO-8601 date/time: \(text)")
            }
            self.init(handle: dt)
        }

        public init(handle: OpaquePointer) {
            self.handle = handle
        }

        deinit {
            g_date_time_unref(handle)
        }

        public var description: String {
            "GLib.DateTime()"
        }

        public static func == (lhs: DateTime, rhs: DateTime) -> Bool {
            g_date_time_compare(gpointer(lhs.handle), gpointer(rhs.handle)) == 0
        }

        public func hash(into hasher: inout Hasher) {
            hasher.combine(g_date_time_hash(gpointer(handle)))
        }

        public var iso8601String: String {
            let raw = g_date_time_format_iso8601(handle)!
            defer {
                g_free(raw)
            }
            return String(cString: raw)
        }
    }
}
