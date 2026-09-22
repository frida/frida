    public func forEachHeader(_ callback: (String, String) -> Void) {
        typealias Visitor = (String, String) -> Void
        withoutActuallyEscaping(callback) { escaping in
            var box = escaping
            withUnsafeMutablePointer(to: &box) { boxPtr in
                frida_web_request_foreach_header(handle, { namePtr, valuePtr, userData in
                    let cbPtr = userData!.assumingMemoryBound(to: Visitor.self)
                    cbPtr.pointee(String(cString: namePtr!), String(cString: valuePtr!))
                }, UnsafeMutableRawPointer(boxPtr))
            }
        }
    }
