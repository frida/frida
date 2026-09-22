internal import FridaCore

class Marshal {
    private static let gvariantByteArrayType = g_variant_type_new("ay")
    private static let gvariantVarDictType = g_variant_type_new("a{sv}")
    private static let gvariantByteType = g_variant_type_new("y")
    private static let gvariantVariantArrayType = g_variant_type_new("av")

    static func takeNativeError(_ error: UnsafeMutablePointer<GError>) -> Swift.Error {
        let domain = error.pointee.domain
        let code = error.pointee.code
        let message = String(cString: error.pointee.message)

        g_error_free(error)

        if domain == g_io_error_quark() &&
           code == Int32(G_IO_ERROR_CANCELLED.rawValue) {
            return CancellationError()
        }

        if domain == frida_error_quark() {
            let fridaCode = FridaError(numericCast(code))

            switch fridaCode {
            case FRIDA_ERROR_SERVER_NOT_RUNNING:
                return Error.serverNotRunning(message)
            case FRIDA_ERROR_EXECUTABLE_NOT_FOUND:
                return Error.executableNotFound(message)
            case FRIDA_ERROR_EXECUTABLE_NOT_SUPPORTED:
                return Error.executableNotSupported(message)
            case FRIDA_ERROR_PROCESS_NOT_FOUND:
                return Error.processNotFound(message)
            case FRIDA_ERROR_PROCESS_NOT_RESPONDING:
                return Error.processNotResponding(message)
            case FRIDA_ERROR_INVALID_ARGUMENT:
                return Error.invalidArgument(message)
            case FRIDA_ERROR_INVALID_OPERATION:
                return Error.invalidOperation(message)
            case FRIDA_ERROR_PERMISSION_DENIED:
                return Error.permissionDenied(message)
            case FRIDA_ERROR_ADDRESS_IN_USE:
                return Error.addressInUse(message)
            case FRIDA_ERROR_TIMED_OUT:
                return Error.timedOut(message)
            case FRIDA_ERROR_NOT_SUPPORTED:
                return Error.notSupported(message)
            case FRIDA_ERROR_PROTOCOL:
                return Error.protocolViolation(message)
            case FRIDA_ERROR_TRANSPORT:
                return Error.transport(message)
            default:
                fatalError("Unexpected Frida error code \(code)")
            }
        }

        return Error.invalidArgument(message)
    }

    static func dictionaryFromParametersDict(_ hashTable: OpaquePointer) -> [String: Any] {
        var result: [String: Any] = [:]

        var iter = GHashTableIter()
        g_hash_table_iter_init(&iter, hashTable)

        var rawKey: gpointer?
        var rawValue: gpointer?
        while g_hash_table_iter_next(&iter, &rawKey, &rawValue) != 0 {
            let key = String(cString: UnsafeRawPointer(rawKey!).assumingMemoryBound(to: Int8.self))
            let value = valueFromVariant(OpaquePointer(rawValue!))
            result[key] = value
        }

        return result
    }

    static func valueFromVariant(_ v: OpaquePointer) -> Any {
        switch g_variant_classify(v) {
        case G_VARIANT_CLASS_STRING:
            return stringFromVariant(v)
        case G_VARIANT_CLASS_BYTE:
            return UInt8(g_variant_get_byte(v))
        case G_VARIANT_CLASS_INT16:
            return Int16(g_variant_get_int16(v))
        case G_VARIANT_CLASS_UINT16:
            return UInt16(g_variant_get_uint16(v))
        case G_VARIANT_CLASS_INT32:
            return Int32(g_variant_get_int32(v))
        case G_VARIANT_CLASS_UINT32:
            return UInt32(g_variant_get_uint32(v))
        case G_VARIANT_CLASS_INT64:
            return Int64(g_variant_get_int64(v))
        case G_VARIANT_CLASS_UINT64:
            return UInt64(g_variant_get_uint64(v))
        case G_VARIANT_CLASS_DOUBLE:
            return Double(g_variant_get_double(v))
        case G_VARIANT_CLASS_BOOLEAN:
            return g_variant_get_boolean(v) != 0
        case G_VARIANT_CLASS_ARRAY:
            if g_variant_is_of_type(v, gvariantByteArrayType) != 0 {
                return byteArrayFromVariant(v)
            }
            if g_variant_is_of_type(v, gvariantVarDictType) != 0 {
                return dictFromVariant(v)
            }
            return arrayFromVariant(v)
        case G_VARIANT_CLASS_TUPLE:
            return tupleFromVariant(v)
        case G_VARIANT_CLASS_VARIANT:
            let inner = g_variant_get_variant(v)!
            let result = valueFromVariant(inner)
            g_variant_unref(inner)
            return result
        default:
            return MarshalNull.shared
        }
    }

    private static func stringFromVariant(_ v: OpaquePointer) -> String {
        return String(cString: UnsafeRawPointer(g_variant_get_string(v, nil)!).assumingMemoryBound(to: Int8.self))
    }

    private static func byteArrayFromVariant(_ v: OpaquePointer) -> [UInt8] {
        var count: gsize = 0
        let basePtr = g_variant_get_fixed_array(v, &count, 1)!

        let length = Int(count)
        var bytes = [UInt8](repeating: 0, count: length)
        _ = bytes.withUnsafeMutableBytes { dst in
            memcpy(dst.baseAddress!, basePtr, length)
        }
        return bytes
    }

    private static func dictFromVariant(_ v: OpaquePointer) -> [String: Any] {
        var result: [String: Any] = [:]

        var iter = GVariantIter()
        g_variant_iter_init(&iter, v)

        while let entry = g_variant_iter_next_value(&iter) {
            let rawKey = g_variant_get_child_value(entry, 0)!
            let rawValue = g_variant_get_child_value(entry, 1)!

            let key = stringFromVariant(rawKey)
            let value = valueFromVariant(rawValue)
            result[key] = value

            g_variant_unref(rawValue)
            g_variant_unref(rawKey)
            g_variant_unref(entry)
        }

        return result
    }

    private static func arrayFromVariant(_ v: OpaquePointer) -> [Any] {
        var result: [Any] = []

        var iter = GVariantIter()
        g_variant_iter_init(&iter, v)

        while let child = g_variant_iter_next_value(&iter) {
            result.append(valueFromVariant(child))
            g_variant_unref(child)
        }

        return result
    }

    private static func tupleFromVariant(_ v: OpaquePointer) -> [Any] {
        let n = g_variant_n_children(v)
        var result: [Any] = []
        result.reserveCapacity(Int(n))

        for i in 0..<n {
            let child = g_variant_get_child_value(v, i)!
            result.append(valueFromVariant(child))
            g_variant_unref(child)
        }

        return result
    }

    static func variantFromValue(_ value: Any) -> OpaquePointer {
        switch value {
        case let string as String:
            return g_variant_new_string(string)
        case let flag as Bool:
            return g_variant_new_boolean(flag ? gboolean(1) : gboolean(0))
        case let number as Int:
            return g_variant_new_int64(gint64(number))
        case let number as Int64:
            return g_variant_new_int64(gint64(number))
        case let number as UInt:
            return g_variant_new_uint64(guint64(number))
        case let number as UInt64:
            return g_variant_new_uint64(guint64(number))
        case let number as Double:
            return g_variant_new_double(number)
        case let bytes as [UInt8]:
            return byteArrayVariant(bytes)
        case let dict as [String: Any]:
            return varDictVariant(dict)
        case let array as [Any]:
            return arrayVariant(array)
        default:
            fatalError("Unsupported value for GVariant: \(value)")
        }
    }

    private static func byteArrayVariant(_ bytes: [UInt8]) -> OpaquePointer {
        return bytes.withUnsafeBufferPointer { buffer in
            g_variant_new_fixed_array(gvariantByteType, buffer.baseAddress, gsize(bytes.count), 1)
        }
    }

    private static func varDictVariant(_ dict: [String: Any]) -> OpaquePointer {
        let builder = g_variant_builder_new(gvariantVarDictType)
        for (key, value) in dict {
            let entry = g_variant_new_dict_entry(
                g_variant_new_string(key),
                g_variant_new_variant(variantFromValue(value))
            )
            g_variant_builder_add_value(builder, entry)
        }
        let result = g_variant_builder_end(builder)!
        g_variant_builder_unref(builder)
        return result
    }

    private static func arrayVariant(_ array: [Any]) -> OpaquePointer {
        let builder = g_variant_builder_new(gvariantVariantArrayType)
        for value in array {
            g_variant_builder_add_value(builder, g_variant_new_variant(variantFromValue(value)))
        }
        let result = g_variant_builder_end(builder)!
        g_variant_builder_unref(builder)
        return result
    }

    static func variantFromIcon(_ icon: Icon) -> OpaquePointer {
        switch icon {
        case .png(let data):
            var bytes = data
            return bytes.withUnsafeMutableBufferPointer { image in
                frida_icon_from_png(image.baseAddress, gint(image.count), 0, 0)
            }
        case .rgba(let width, let height, let pixels):
            var bytes = pixels
            return bytes.withUnsafeMutableBufferPointer { image in
                frida_icon_from_rgba(image.baseAddress, gint(image.count), guint16(width), guint16(height))
            }
        }
    }

    static func iconFromVarDict(_ dict: [String: Any]) -> Icon {
        let formatString = dict["format"] as! String
        let bytes = dict["image"] as! [UInt8]

        switch formatString {
        case "rgba":
            let w = dict["width"] as! UInt16
            let h = dict["height"] as! UInt16

            return .rgba(
                width: Int(w),
                height: Int(h),
                pixels: bytes
            )

        case "png":
            return .png(
                data: bytes
            )

        default:
            fatalError("Unexpected icon format from Frida: \(formatString)")
        }
    }

    static func arrayFromStrv(_ strv: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>) -> [String] {
        var result: [String] = []

        var cursor = strv
        while let str = cursor.pointee {
            result.append(String(cString: str))
            cursor += 1
        }

        return result
    }

    static func strvFromArray(_ array: [String]?) -> (UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>?, gint) {
        var strv: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>?
        var length: gint

        if let array = array {
            strv = unsafeBitCast(g_malloc0(gsize((array.count + 1) * MemoryLayout<gpointer>.size)), to: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>.self)
            for (index, element) in array.enumerated() {
                strv!.advanced(by: index).pointee = g_strdup(element)
            }
            length = gint(array.count)
        } else {
            strv = nil
            length = -1
        }

        return (strv, length)
    }

    static func dictionaryFromEnvp(_ envp: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>) -> [String: String] {
        var result: [String: String] = [:]
        for pair in arrayFromStrv(envp) {
            let tokens = pair.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            let key: String = String(tokens[0])
            let value: String = String(tokens[1])
            result[key] = value
        }
        return result
    }

    static func envpFromDictionary(_ dict: [String: String]?) -> (UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>?, gint) {
        var envp: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>?
        var length: gint

        if let dict = dict {
            envp = unsafeBitCast(g_malloc0(gsize((dict.count + 1) * MemoryLayout<gpointer>.size)), to: UnsafeMutablePointer<UnsafeMutablePointer<gchar>?>.self)
            for item in dict.enumerated() {
                envp!.advanced(by: item.offset).pointee = g_strdup(item.element.key + "=" + item.element.value)
            }
            length = gint(dict.count)
        } else {
            envp = nil
            length = -1
        }

        return (envp, length)
    }

    public static func bytesFromArray(_ data: [UInt8]?) -> OpaquePointer? {
        guard let data = data else {
            return nil
        }

        return data.withUnsafeBytes { rawBuf -> OpaquePointer? in
            guard let base = rawBuf.baseAddress else {
                return nil
            }

            let size = gsize(rawBuf.count)
            let bytesHandle = g_bytes_new(base, size)

            return bytesHandle
        }
    }

    public static func arrayFromBytes(_ bytesHandle: OpaquePointer?) -> [UInt8]? {
        guard let bytesHandle = bytesHandle else {
            return nil
        }

        var size: gsize = 0

        let rawPtr = g_bytes_get_data(bytesHandle, &size)

        guard let rawPtr = rawPtr else {
            return []
        }

        let count = Int(size)
        if count == 0 {
            return []
        }

        var result = [UInt8](repeating: 0, count: count)

        _ = result.withUnsafeMutableBytes { dstBuf in
            memcpy(dstBuf.baseAddress!, rawPtr, count)
        }

        return result
    }

    static func jsonFromValue(_ value: Any) -> String {
        return try! JSONGLib.string(from: value)
    }

    static func valueFromJSON(_ json: String) throws -> Any {
        return try! JSONGLib.value(from: json)
    }

    static func certificateFromString(_ string: String) throws -> UnsafeMutablePointer<GTlsCertificate> {
        var result: UnsafeMutablePointer<GTlsCertificate>?
        var rawError: UnsafeMutablePointer<GError>? = nil
        if string.contains("\n") {
            result = g_tls_certificate_new_from_pem(string, -1, &rawError)
        } else {
            result = g_tls_certificate_new_from_file(string, &rawError)
        }
        if let rawError = rawError {
            let message = String(cString: rawError.pointee.message)
            g_error_free(rawError)
            throw Error.invalidArgument(message)
        }
        return result!
    }
}

public struct MarshalNull: CustomStringConvertible, Equatable {
    public static let shared = MarshalNull()

    private init() {}

    public var description: String { "null" }
}
