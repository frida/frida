internal import FridaCore


extension GLib {
    public final class DBusConnection: @unchecked Sendable {
        private let handle: OpaquePointer
        private var stream: UnsafeMutablePointer<GIOStream>?
        private var socket: Socket?
        private var registrations: [DBusRegistration] = []

        private init(handle: OpaquePointer) {
            self.handle = handle
        }

        deinit {
            registrations.forEach { $0.invalidate() }
            g_object_unref(gpointer(handle))
            if let stream {
                g_object_unref(gpointer(stream))
            }
        }

        public static func connect(
            to socket: Socket,
            as role: DBusRole,
            startingMessageProcessing: Bool = true
        ) async throws -> DBusConnection {
            let connection = g_socket_connection_factory_create_connection(socket.handle)
            let stream = UnsafeMutableRawPointer(connection!).assumingMemoryBound(to: GIOStream.self)

            let dbus = try await fridaAsync(DBusConnection.self) { op in
                var flags = role.flags
                if !startingMessageProcessing {
                    flags |= DBusConnectionFlag.delayMessageProcessing
                }

                g_dbus_connection_new(stream, role.guid, glibFlags(flags), nil, op.cancellable, { sourcePtr, asyncResultPtr, userDataPtr in
                    let op = InternalOp<DBusConnection>.takeRetained(from: userDataPtr!)

                    var rawError: UnsafeMutablePointer<GError>? = nil
                    let handle = g_dbus_connection_new_finish(asyncResultPtr, &rawError)

                    if let rawError {
                        op.resumeFailure(Marshal.takeNativeError(rawError))
                        return
                    }

                    op.resumeSuccess(DBusConnection(handle: handle!))
                }, op.userData)
            }

            dbus.stream = stream
            dbus.socket = socket
            return dbus
        }

        public func startMessageProcessing() {
            MainContext.frida.schedule {
                g_dbus_connection_start_message_processing(self.handle)
            }
        }

        public func registerObject(at path: String, interfaces xml: String, handler: any DBusObjectHandler) async throws {
            try await MainContext.frida.perform {
                try self.register(at: path, interfaces: xml, handler: handler)
            }
        }

        private func register(at path: String, interfaces xml: String, handler: any DBusObjectHandler) throws {
            var rawError: UnsafeMutablePointer<GError>? = nil
            let info = g_dbus_node_info_new_for_xml(xml, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            defer { g_dbus_node_info_unref(info) }

            var index = 0
            while let interface = info!.pointee.interfaces[index] {
                let registration = DBusRegistration(handler: handler)
                g_dbus_connection_register_object(
                    handle,
                    path,
                    interface,
                    registration.vtable,
                    registration.userData,
                    { userData in
                        Unmanaged<DBusRegistration>.fromOpaque(userData!).release()
                    },
                    &rawError
                )
                if let rawError {
                    throw Marshal.takeNativeError(rawError)
                }
                registrations.append(registration)
                index += 1
            }
        }

        public func call(
            at path: String,
            interface: String,
            method: String,
            parameters: Variant? = nil,
            fileDescriptors: [Int32] = []
        ) async throws -> Variant {
            #if os(Windows)
            try await fridaAsync(Variant.self) { op in
                g_dbus_connection_call(
                    self.handle,
                    nil,
                    path,
                    interface,
                    method,
                    parameters?.handle,
                    nil,
                    glibFlags(0),
                    -1,
                    op.cancellable,
                    { sourcePtr, asyncResultPtr, userDataPtr in
                        let op = InternalOp<Variant>.takeRetained(from: userDataPtr!)

                        var rawError: UnsafeMutablePointer<GError>? = nil
                        let reply = g_dbus_connection_call_finish(
                            OpaquePointer(sourcePtr), asyncResultPtr, &rawError)

                        if let rawError {
                            op.resumeFailure(Marshal.takeNativeError(rawError))
                            return
                        }

                        let result = Variant(borrowing: reply!)
                        g_variant_unref(reply)
                        op.resumeSuccess(result)
                    },
                    op.userData
                )
            }
            #else
            try await fridaAsync(Variant.self) { op in
                let list = g_unix_fd_list_new()
                defer { g_object_unref(gpointer(list)) }
                for fileDescriptor in fileDescriptors {
                    g_unix_fd_list_append(list, fileDescriptor, nil)
                }

                g_dbus_connection_call_with_unix_fd_list(
                    self.handle,
                    nil,
                    path,
                    interface,
                    method,
                    parameters?.handle,
                    nil,
                    glibFlags(0),
                    -1,
                    list,
                    op.cancellable,
                    { sourcePtr, asyncResultPtr, userDataPtr in
                        let op = InternalOp<Variant>.takeRetained(from: userDataPtr!)

                        var rawError: UnsafeMutablePointer<GError>? = nil
                        let reply = g_dbus_connection_call_with_unix_fd_list_finish(
                            OpaquePointer(sourcePtr), nil, asyncResultPtr, &rawError)

                        if let rawError {
                            op.resumeFailure(Marshal.takeNativeError(rawError))
                            return
                        }

                        let result = Variant(borrowing: reply!)
                        g_variant_unref(reply)
                        op.resumeSuccess(result)
                    },
                    op.userData
                )
            }
            #endif
        }

        public func close() {
            g_dbus_connection_close(handle, nil, nil, nil)
        }
    }

    public enum DBusRole: Sendable {
        case client
        case server(guid: String)

        var flags: UInt32 {
            switch self {
            case .client:
                return DBusConnectionFlag.authenticationClient
            case .server:
                return DBusConnectionFlag.authenticationServer | DBusConnectionFlag.authenticationAllowAnonymous
            }
        }

        var guid: String? {
            switch self {
            case .client:
                return nil
            case .server(let guid):
                return guid
            }
        }
    }

    public protocol DBusObjectHandler: AnyObject, Sendable {
        func handle(_ call: DBusMethodCall)
        func property(_ name: String, on interface: String) -> Variant?
    }

    public final class DBusMethodCall {
        public let interface: String
        public let method: String
        public let parameters: Variant

        private let invocation: OpaquePointer

        init(interface: String, method: String, parameters: Variant, invocation: OpaquePointer) {
            self.interface = interface
            self.method = method
            self.parameters = parameters
            self.invocation = invocation
        }

        #if !os(Windows)
        public func fileDescriptor(at index: Int32) throws -> Int32 {
            let message = g_dbus_method_invocation_get_message(invocation)
            guard let list = g_dbus_message_get_unix_fd_list(message) else {
                throw Frida.Error.protocolViolation("\(method) carried no file descriptors")
            }

            var rawError: UnsafeMutablePointer<GError>? = nil
            let fileDescriptor = g_unix_fd_list_get(list, index, &rawError)
            if let rawError {
                throw Marshal.takeNativeError(rawError)
            }
            return fileDescriptor
        }
        #endif

        public func complete() {
            g_dbus_method_invocation_return_value(invocation, nil)
        }

        public func fail(_ message: String) {
            g_dbus_method_invocation_return_error_literal(
                invocation, g_io_error_quark(), gint(G_IO_ERROR_FAILED.rawValue), message)
        }
    }
}

private enum DBusConnectionFlag {
    static let authenticationClient: UInt32 = 1 << 0
    static let authenticationServer: UInt32 = 1 << 1
    static let authenticationAllowAnonymous: UInt32 = 1 << 2
    static let delayMessageProcessing: UInt32 = 1 << 4
}

private func glibFlags<Flags>(_ value: UInt32) -> Flags {
    unsafeBitCast(value, to: Flags.self)
}

private final class DBusRegistration: @unchecked Sendable {
    weak var handler: (any GLib.DBusObjectHandler)?

    let vtable: UnsafeMutablePointer<GDBusInterfaceVTable>

    init(handler: any GLib.DBusObjectHandler) {
        self.handler = handler
        self.vtable = .allocate(capacity: 1)
        vtable.initialize(to: GDBusInterfaceVTable(
            method_call: { _, _, _, interfaceName, methodName, parameters, invocation, userData in
                let registration = Unmanaged<DBusRegistration>.fromOpaque(userData!).takeUnretainedValue()
                registration.handler?.handle(
                    GLib.DBusMethodCall(
                        interface: String(cString: interfaceName!),
                        method: String(cString: methodName!),
                        parameters: GLib.Variant(borrowing: parameters!),
                        invocation: invocation!
                    )
                )
            },
            get_property: { _, _, _, interfaceName, propertyName, _, userData in
                let registration = Unmanaged<DBusRegistration>.fromOpaque(userData!).takeUnretainedValue()
                guard let value = registration.handler?.property(
                    String(cString: propertyName!),
                    on: String(cString: interfaceName!)
                ) else {
                    return nil
                }
                return g_variant_ref(value.handle)
            },
            set_property: nil,
            padding: (nil, nil, nil, nil, nil, nil, nil, nil)
        ))
    }

    deinit {
        vtable.deinitialize(count: 1)
        vtable.deallocate()
    }

    var userData: UnsafeMutableRawPointer {
        Unmanaged.passRetained(self).toOpaque()
    }

    func invalidate() {
        handler = nil
    }
}

