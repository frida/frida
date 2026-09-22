internal import FridaCore

extension GLib {
    public final class TlsCertificate: @unchecked Sendable, CustomStringConvertible, Equatable, Hashable {
        internal let handle: OpaquePointer

        public convenience init(file: String) throws {
            Runtime.ensureInitialized()

            var rawError: UnsafeMutablePointer<GError>? = nil
            let raw = g_tls_certificate_new_from_file(file, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            self.init(handle: OpaquePointer(raw!))
        }

        public convenience init(pem: String) throws {
            Runtime.ensureInitialized()

            var rawError: UnsafeMutablePointer<GError>? = nil
            let raw = g_tls_certificate_new_from_pem(pem, -1, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            self.init(handle: OpaquePointer(raw!))
        }

        public init(handle: OpaquePointer) {
            self.handle = handle
        }

        deinit {
            g_object_unref(gpointer(handle))
        }

        public var description: String {
            "GLib.TlsCertificate()"
        }

        public static func == (lhs: TlsCertificate, rhs: TlsCertificate) -> Bool {
            lhs.handle == rhs.handle
        }

        public func hash(into hasher: inout Hasher) {
            hasher.combine(UInt(bitPattern: handle))
        }
    }
}
