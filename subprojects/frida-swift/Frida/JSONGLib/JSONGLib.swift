internal import FridaCore
#if canImport(Darwin)
import Foundation
#endif

public enum JSONGLib {
    public static func string(from value: Any, pretty: Bool = false) throws -> String {
        let root = try makeNode(from: value)
        defer { json_node_free(root) }

        let raw = json_to_string(root, pretty ? 1 : 0)!

        let result = String(cString: raw)
        g_free(raw)
        return result
    }

    public static func value(from string: String) throws -> Any {
        var rawError: UnsafeMutablePointer<GError>? = nil
        guard let node = json_from_string(string, &rawError) else {
            throw Marshal.takeNativeError(rawError!)
        }
        defer { json_node_unref(node) }

        return decode(node)
    }

    private static func makeNode(from value: Any) throws -> OpaquePointer {
        if value is JSONNull {
            return makeNullNode()
        }

        if let dict = value as? [String: Any] {
            return try makeObjectNode(from: dict)
        }

        if let array = value as? [Any] {
            return try makeArrayNode(from: array)
        }

        #if canImport(Darwin)
        if let num = value as? NSNumber {
            return makeNodeFromNSNumber(num)
        }
        #endif

        if let b = value as? Bool {
            return makeBoolNode(b)
        }

        if let i = value as? Int {
            return makeIntNode(Int64(i))
        }
        if let i = value as? Int8  { return makeIntNode(Int64(i)) }
        if let i = value as? Int16 { return makeIntNode(Int64(i)) }
        if let i = value as? Int32 { return makeIntNode(Int64(i)) }
        if let i = value as? Int64 { return makeIntNode(i) }

        if let u = value as? UInt {
            return makeIntNode(Int64(u))
        }
        if let u = value as? UInt8  { return makeIntNode(Int64(u)) }
        if let u = value as? UInt16 { return makeIntNode(Int64(u)) }
        if let u = value as? UInt32 { return makeIntNode(Int64(u)) }
        if let u = value as? UInt64 {
            let clamped = (u > UInt64(Int64.max)) ? Int64.max : Int64(u)
            return makeIntNode(clamped)
        }

        if let d = value as? Double {
            return makeDoubleNode(d)
        }
        if let f = value as? Float {
            return makeDoubleNode(Double(f))
        }

        if let s = value as? String {
            return makeStringNode(s)
        }

        throw Error.invalidArgument("Unsupported value type")
    }

    #if canImport(Darwin)
    private static func makeNodeFromNSNumber(_ num: NSNumber) -> OpaquePointer {
        if CFGetTypeID(num) == CFBooleanGetTypeID() {
            return makeBoolNode(num.boolValue)
        }
        switch UnicodeScalar(UInt8(num.objCType.pointee)) {
        case "f", "d":
            return makeDoubleNode(num.doubleValue)
        default:
            return makeIntNode(num.int64Value)
        }
    }
    #endif

    private static func makeObjectNode(from dict: [String: Any]) throws -> OpaquePointer {
        let node = json_node_new(JSON_NODE_OBJECT)!
        let object = json_object_new()

        json_node_take_object(node, object)

        for (key, rawValue) in dict {
            let child = try makeNode(from: rawValue)
            key.withCString { cStr in
                json_object_set_member(object, cStr, child)
            }
        }

        return node
    }

    private static func makeArrayNode(from array: [Any]) throws -> OpaquePointer {
        let node = json_node_new(JSON_NODE_ARRAY)!
        let jsonArray = json_array_new()

        json_node_take_array(node, jsonArray)

        for element in array {
            let child = try makeNode(from: element)
            json_array_add_element(jsonArray, child)
        }

        return node
    }

    private static func makeBoolNode(_ value: Bool) -> OpaquePointer {
        let node = json_node_new(JSON_NODE_VALUE)!
        json_node_set_boolean(node, value ? 1 : 0)
        return node
    }

    private static func makeIntNode(_ value: Int64) -> OpaquePointer {
        let node = json_node_new(JSON_NODE_VALUE)!
        json_node_set_int(node, gint64(value))
        return node
    }

    private static func makeDoubleNode(_ value: Double) -> OpaquePointer {
        let node = json_node_new(JSON_NODE_VALUE)!
        json_node_set_double(node, value)
        return node
    }

    private static func makeStringNode(_ value: String) -> OpaquePointer {
        let node = json_node_new(JSON_NODE_VALUE)!
        value.withCString { cStr in
            json_node_set_string(node, cStr)
        }
        return node
    }

    private static func makeNullNode() -> OpaquePointer {
        let node = json_node_new(JSON_NODE_NULL)!
        return node
    }

    private static func decode(_ node: OpaquePointer) -> Any {
        switch json_node_get_node_type(node) {
        case JSON_NODE_OBJECT:
            return decodeObjectNode(node)
        case JSON_NODE_ARRAY:
            return decodeArrayNode(node)
        case JSON_NODE_VALUE:
            return decodeValueNode(node)
        case JSON_NODE_NULL:
            return JSONNull.null
        default:
            fatalError("Invalid node type")
        }
    }

    private static func decodeObjectNode(_ node: OpaquePointer) -> Any {
        let obj = json_node_get_object(node)

        let members = json_object_get_members(obj)
        defer { g_list_free(members) }

        var result: [String: Any] = [:]
        var iter = members
        while let cur = iter {
            let keyPtr = cur.pointee.data.assumingMemoryBound(to: CChar.self)
            result[String(cString: keyPtr)] = decode(json_object_get_member(obj, keyPtr))
            iter = cur.pointee.next
        }
        return result
    }

    private static func decodeArrayNode(_ node: OpaquePointer) -> Any {
        let array = json_node_get_array(node)

        var result: [Any] = []
        let n = json_array_get_length(array)
        result.reserveCapacity(Int(n))

        var i: guint = 0
        while i != n {
            result.append(decode(json_array_get_element(array, i)) as Any)
            i &+= 1
        }
        return result
    }

    private static func decodeValueNode(_ node: OpaquePointer) -> Any {
        switch json_node_get_value_type(node) {
        case GType.boolean:
            return json_node_get_boolean(node) != 0
        case GType.int64:
            let raw = json_node_get_int(node)
            if raw >= Int64(Int.min) && raw <= Int64(Int.max) {
                return Int(raw)
            } else {
                return Double(raw)
            }
        case GType.double:
            return json_node_get_double(node)
        case GType.string:
            return String(cString: json_node_get_string(node))
        default:
            fatalError("Invalid value type")
        }
    }
}

#if canImport(Foundation)
import Foundation

public typealias JSONNull = NSNull

public extension JSONNull {
    static var null: JSONNull { JSONNull() }
}
#else
public enum JSONNull: CustomStringConvertible, Equatable, Hashable {
    case null

    public var description: String {
        "null"
    }
}
#endif
