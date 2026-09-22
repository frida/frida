internal import FridaCore

#if canImport(Darwin)
import Darwin
#elseif canImport(Glibc)
import Glibc
#elseif canImport(WinSDK)
import WinSDK
#endif

extension GLib {
    public final class Socket: @unchecked Sendable {
        let handle: UnsafeMutablePointer<GSocket>

        init(handle: UnsafeMutablePointer<GSocket>) {
            self.handle = handle
        }

        deinit {
            g_object_unref(gpointer(handle))
        }

        public convenience init(adopting fileDescriptor: Int32) throws {
            var rawError: UnsafeMutablePointer<GError>? = nil
            let handle = g_socket_new_from_fd(fileDescriptor, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            self.init(handle: handle!)
        }

        public static func connect(unixPath: String) throws -> Socket {
            let address = g_unix_socket_address_new(unixPath)!
            defer { g_object_unref(gpointer(address)) }

            var rawError: UnsafeMutablePointer<GError>? = nil
            let handle = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }

            let socket = Socket(handle: handle!)
            g_socket_connect(handle, UnsafeMutableRawPointer(address).assumingMemoryBound(to: GSocketAddress.self), nil, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            return socket
        }

        #if os(Windows)
        public static func pair() throws -> (Socket, Socket) {
            let directory = String(cString: g_get_tmp_dir())
            let name = g_uuid_string_random()!
            defer { g_free(name) }
            let path = "\(directory)\\frida-\(String(cString: name))"

            let address = g_unix_socket_address_new(path)!
            defer { g_object_unref(gpointer(address)) }
            let socketAddress = UnsafeMutableRawPointer(address).assumingMemoryBound(to: GSocketAddress.self)

            var rawError: UnsafeMutablePointer<GError>? = nil
            let listener = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            defer {
                g_socket_close(listener, nil)
                g_object_unref(gpointer(listener))
                path.withCString { _ = remove($0) }
            }

            g_socket_bind(listener, socketAddress, gboolean(1), &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            g_socket_listen(listener, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }

            let client = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            g_socket_connect(client, socketAddress, nil, &rawError)
            if let rawError {
                g_object_unref(gpointer(client))
                throw Marshal.takeNativeError(rawError)
            }

            let server = g_socket_accept(listener, nil, &rawError)
            if let rawError {
                g_object_unref(gpointer(client))
                throw Marshal.takeNativeError(rawError)
            }

            return (Socket(handle: client!), Socket(handle: server!))
        }

        public func protocolInfo(forProcessID processID: UInt32) throws -> [UInt8] {
            var info = WSAPROTOCOL_INFOW()
            let socket = SOCKET(bitPattern: Int64(g_socket_get_fd(handle)))
            guard WSADuplicateSocketW(socket, DWORD(processID), &info) == 0 else {
                throw Frida.Error.transport("Unable to duplicate socket: \(WSAGetLastError())")
            }
            return withUnsafeBytes(of: &info) { Array($0) }
        }
        #else
        public static func pair() throws -> (Socket, Socket) {
            var descriptors: [Int32] = [-1, -1]
            guard socketpair(AF_UNIX, streamSocketTypeValue, 0, &descriptors) == 0 else {
                throw Frida.Error.transport("Unable to make a socket pair: \(String(cString: strerror(errno)))")
            }
            return (try Socket(adopting: descriptors[0]), try Socket(adopting: descriptors[1]))
        }

        #if canImport(Darwin)
        private static let streamSocketTypeValue = SOCK_STREAM
        #else
        private static let streamSocketTypeValue = Int32(SOCK_STREAM.rawValue)
        #endif
        #endif

        public var fileDescriptor: Int32 {
            g_socket_get_fd(handle)
        }

        #if os(Windows)
        public func send(_ bytes: [UInt8], fileDescriptors: [Int32] = []) throws {
            var payload = bytes
            var rawError: UnsafeMutablePointer<GError>? = nil
            payload.withUnsafeBytes { payload in
                _ = g_socket_send(handle, payload.baseAddress, gsize(payload.count), nil, &rawError)
            }
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
        }
        #else
        public func send(_ bytes: [UInt8], fileDescriptors: [Int32] = []) throws {
            var payload = bytes
            var control = [UInt8](repeating: 0, count: Self.controlLength(for: fileDescriptors))

            let sent: Int = payload.withUnsafeMutableBufferPointer { payload in
                control.withUnsafeMutableBufferPointer { control in
                    var vector = iovec(iov_base: payload.baseAddress, iov_len: payload.count)
                    return withUnsafeMutablePointer(to: &vector) { vector in
                        var message = msghdr()
                        message.msg_iov = vector
                        message.msg_iovlen = 1

                        if !fileDescriptors.isEmpty {
                            Self.lay(fileDescriptors, into: control)
                            message.msg_control = UnsafeMutableRawPointer(control.baseAddress)
                            message.msg_controllen = .init(control.count)
                        }

                        return sendmsg(fileDescriptor, &message, 0)
                    }
                }
            }
            guard sent >= 0 else {
                throw Frida.Error.transport("Unable to send: \(String(cString: strerror(errno)))")
            }
        }

        private static func lay(_ fileDescriptors: [Int32], into control: UnsafeMutableBufferPointer<UInt8>) {
            let header = UnsafeMutableRawPointer(control.baseAddress!).assumingMemoryBound(to: cmsghdr.self)
            header.pointee.cmsg_level = SOL_SOCKET
            header.pointee.cmsg_type = .init(SCM_RIGHTS)
            header.pointee.cmsg_len = .init(headerLength + payloadLength(for: fileDescriptors))

            let payload = UnsafeMutableRawPointer(control.baseAddress!)
                .advanced(by: headerLength)
                .assumingMemoryBound(to: Int32.self)
            for (index, fileDescriptor) in fileDescriptors.enumerated() {
                payload[index] = fileDescriptor
            }
        }

        private static func controlLength(for fileDescriptors: [Int32]) -> Int {
            guard !fileDescriptors.isEmpty else { return 0 }
            return aligned(headerLength + payloadLength(for: fileDescriptors))
        }

        private static func payloadLength(for fileDescriptors: [Int32]) -> Int {
            fileDescriptors.count * MemoryLayout<Int32>.size
        }

        private static let headerLength = aligned(MemoryLayout<cmsghdr>.size)

        private static func aligned(_ length: Int) -> Int {
            #if canImport(Darwin)
            let boundary = MemoryLayout<UInt32>.size
            #else
            let boundary = MemoryLayout<Int>.size
            #endif
            return (length + boundary - 1) & ~(boundary - 1)
        }
        #endif

        public func receive(upTo count: Int) throws -> [UInt8] {
            var buffer = [UInt8](repeating: 0, count: count)
            var rawError: UnsafeMutablePointer<GError>? = nil
            let received = buffer.withUnsafeMutableBytes { buffer in
                g_socket_receive(handle, buffer.baseAddress, gsize(buffer.count), nil, &rawError)
            }
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            return Array(buffer[0..<max(Int(received), 0)])
        }

        public func close() {
            g_socket_close(handle, nil)
        }
    }
}

