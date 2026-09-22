internal import FridaCore

@discardableResult
func connectSignal<Handler>(
    instance: AnyObject,
    handle: OpaquePointer,
    signal: UnsafePointer<CChar>,
    handler: Handler,
    flags: GConnectFlags = GConnectFlags(rawValue: 0)
) -> gulong {
    return g_signal_connect_data(
        gpointer(handle),
        signal,
        unsafeBitCast(handler, to: GCallback.self),
        Unmanaged.passRetained(SignalConnection(instance: instance)).toOpaque(),
        destroySignalConnection,
        flags
    )
}

private let destroySignalConnection: GClosureNotify = { data, _ in
    Unmanaged<AnySignalConnection>.fromOpaque(data!).release()
}

class AnySignalConnection {
    weak var anyInstance: AnyObject?

    init(_ instance: AnyObject) {
        self.anyInstance = instance
    }
}

final class SignalConnection<T: AnyObject>: AnySignalConnection {
    init(instance: T) {
        super.init(instance)
    }

    var instance: T? {
        anyInstance as? T
    }
}
