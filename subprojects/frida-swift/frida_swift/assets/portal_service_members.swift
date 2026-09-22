    public var events: Events {
        eventSource.makeStream()
    }

    public typealias Events = AsyncStream<Event>

    @frozen
    public enum Event: @unchecked Sendable {
        case authenticated(connectionId: ConnectionID, sessionInfo: String)
        case controllerConnected(connectionId: ConnectionID, remoteAddress: SocketAddress)
        case controllerDisconnected(connectionId: ConnectionID, remoteAddress: SocketAddress)
        case message(connectionId: ConnectionID, message: Any, data: [UInt8]?)
        case nodeConnected(connectionId: ConnectionID, remoteAddress: SocketAddress)
        case nodeDisconnected(connectionId: ConnectionID, remoteAddress: SocketAddress)
        case nodeJoined(connectionId: ConnectionID, application: ApplicationDetails)
        case nodeLeft(connectionId: ConnectionID, application: ApplicationDetails)
        case subscribe(connectionId: ConnectionID)
    }

    public typealias ConnectionID = UInt32

    private let eventSource = AsyncEventSource<Event>()

    public struct SocketAddress: CustomStringConvertible {
        internal let handle: OpaquePointer

        public var description: String {
            "SocketAddress(\(Unmanaged<AnyObject>.fromOpaque(UnsafeMutableRawPointer(handle)).toOpaque()))"
        }
    }

    public convenience init(clusterParameters: EndpointParameters, controlParameters: EndpointParameters? = nil) {
        self.init(handle: frida_portal_service_new(clusterParameters.handle, controlParameters?.handle))
    }

    private let onAuthenticated: @convention(c) (OpaquePointer, guint, UnsafePointer<gchar>, gpointer) -> Void = { _, rawConnectionId, rawSessionInfo, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.authenticated(connectionId: ConnectionID(rawConnectionId), sessionInfo: String(cString: rawSessionInfo)))
    }

    private let onControllerConnected: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawSocketAddress, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.controllerConnected(connectionId: ConnectionID(rawConnectionId), remoteAddress: SocketAddress(handle: rawSocketAddress)))
    }

    private let onControllerDisconnected: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawSocketAddress, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.controllerDisconnected(connectionId: ConnectionID(rawConnectionId), remoteAddress: SocketAddress(handle: rawSocketAddress)))
    }

    private let onMessage: @convention(c) (OpaquePointer, guint, UnsafePointer<gchar>, OpaquePointer?, gpointer) -> Void = { _, rawConnectionId, rawJson, rawData, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        guard let parsedAny = try? Marshal.valueFromJSON(String(cString: rawJson)) else { return }
        connection.instance?.publish(.message(connectionId: ConnectionID(rawConnectionId), message: parsedAny, data: Marshal.arrayFromBytes(rawData)))
    }

    private let onNodeConnected: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawSocketAddress, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.nodeConnected(connectionId: ConnectionID(rawConnectionId), remoteAddress: SocketAddress(handle: rawSocketAddress)))
    }

    private let onNodeDisconnected: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawSocketAddress, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.nodeDisconnected(connectionId: ConnectionID(rawConnectionId), remoteAddress: SocketAddress(handle: rawSocketAddress)))
    }

    private let onNodeJoined: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawApplication, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.nodeJoined(connectionId: ConnectionID(rawConnectionId), application: ApplicationDetails(handle: rawApplication)))
    }

    private let onNodeLeft: @convention(c) (OpaquePointer, guint, OpaquePointer, gpointer) -> Void = { _, rawConnectionId, rawApplication, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.nodeLeft(connectionId: ConnectionID(rawConnectionId), application: ApplicationDetails(handle: rawApplication)))
    }

    private let onSubscribe: @convention(c) (OpaquePointer, guint, gpointer) -> Void = { _, rawConnectionId, userData in
        let connection = Unmanaged<SignalConnection<PortalService>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.subscribe(connectionId: ConnectionID(rawConnectionId)))
    }

    private func publish(_ event: Event) {
        eventSource.yield(event)
    }
