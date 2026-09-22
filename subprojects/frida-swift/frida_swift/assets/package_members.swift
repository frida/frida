    static func list(from list: OpaquePointer) -> [Package] {
        let count = Int(frida_package_list_size(list))
        var result: [Package] = []
        result.reserveCapacity(count)
        for i in 0..<count {
            result.append(Package(handle: frida_package_list_get(list, gint(i))))
        }
        return result
    }
