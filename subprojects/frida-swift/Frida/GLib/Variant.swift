internal import FridaCore

extension GLib {
    public final class Variant: CustomStringConvertible {
        let handle: OpaquePointer

        init(adopting handle: OpaquePointer) {
            self.handle = g_variant_ref_sink(handle)
        }

        init(borrowing handle: OpaquePointer) {
            self.handle = g_variant_ref(handle)
        }

        deinit {
            g_variant_unref(handle)
        }

        public convenience init(_ value: UInt32) {
            self.init(adopting: g_variant_new_uint32(value))
        }

        public convenience init(_ value: Int32) {
            self.init(adopting: g_variant_new_int32(value))
        }

        public convenience init(_ value: String) {
            self.init(adopting: g_variant_new_string(value))
        }

        public convenience init(_ values: [String]) {
            let (strv, _) = Marshal.strvFromArray(values)
            defer { g_strfreev(strv) }
            self.init(adopting: UnsafePointer(strv!).withMemoryRebound(to: UnsafePointer<gchar>?.self, capacity: values.count) {
                g_variant_new_strv($0, gssize(values.count))
            })
        }

        public convenience init(bytes: [UInt8]) {
            var payload = bytes
            let elementType = g_variant_type_new("y")
            defer { g_variant_type_free(elementType) }
            self.init(adopting: payload.withUnsafeMutableBufferPointer { payload in
                g_variant_new_fixed_array(elementType, payload.baseAddress, gsize(payload.count), 1)
            })
        }

        public convenience init(tuple children: [Variant]) {
            var handles: [OpaquePointer?] = children.map(\.handle)
            let tuple = withExtendedLifetime(children) {
                handles.withUnsafeMutableBufferPointer { handles in
                    g_variant_new_tuple(handles.baseAddress, gsize(handles.count))
                }
            }
            self.init(adopting: tuple!)
        }

        public static func fileDescriptor(at index: Int32) -> Variant {
            Variant(adopting: g_variant_new_handle(index))
        }

        public var uint32: UInt32? {
            isOfType("u") ? g_variant_get_uint32(handle) : nil
        }

        public var int32: Int32? {
            isOfType("i") ? g_variant_get_int32(handle) : nil
        }

        public var uint64: UInt64? {
            isOfType("t") ? UInt64(g_variant_get_uint64(handle)) : nil
        }

        public var string: String? {
            guard isOfType("s") else { return nil }
            return String(cString: g_variant_get_string(handle, nil))
        }

        public var fileDescriptorIndex: Int32? {
            isOfType("h") ? g_variant_get_handle(handle) : nil
        }

        public var bytes: [UInt8]? {
            guard isOfType("ay") else { return nil }

            var count: gsize = 0
            guard let start = g_variant_get_fixed_array(handle, &count, 1) else { return [] }
            return [UInt8](UnsafeRawBufferPointer(start: start, count: Int(count)))
        }

        public var children: [Variant] {
            (0..<Int(g_variant_n_children(handle))).map { index in
                let child = g_variant_get_child_value(handle, gsize(index))!
                defer { g_variant_unref(child) }
                return Variant(borrowing: child)
            }
        }

        public var description: String {
            let text = g_variant_print(handle, 0)!
            defer { g_free(text) }
            return String(cString: text)
        }

        private func isOfType(_ signature: String) -> Bool {
            String(cString: g_variant_get_type_string(handle)) == signature
        }
    }
}
