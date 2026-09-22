    public var events: Events {
        eventSource.makeStream()
    }

    public typealias Events = AsyncStream<Event>

    @frozen
    public enum Event {
        case starting
        case diagnostics(Any)
        case output(bundle: String)
        case finished
    }

    private let eventSource = AsyncEventSource<Event>()

    public convenience init() {
        Runtime.ensureInitialized()
        self.init(handle: frida_compiler_new(nil))
    }

    private let onStarting: @convention(c) (OpaquePointer, gpointer) -> Void = { _, userData in
        let connection = Unmanaged<SignalConnection<Compiler>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.starting)
    }

    private let onFinished: @convention(c) (OpaquePointer, gpointer) -> Void = { _, userData in
        let connection = Unmanaged<SignalConnection<Compiler>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.finished)
    }

    private let onOutput: @convention(c) (OpaquePointer, UnsafePointer<gchar>, gpointer) -> Void = { _, rawBundle, userData in
        let connection = Unmanaged<SignalConnection<Compiler>>.fromOpaque(userData).takeUnretainedValue()
        connection.instance?.publish(.output(bundle: String(cString: rawBundle)))
    }

    private let onDiagnostics: @convention(c) (OpaquePointer, OpaquePointer?, gpointer) -> Void = { _, rawVariant, userData in
        let connection = Unmanaged<SignalConnection<Compiler>>.fromOpaque(userData).takeUnretainedValue()
        guard let rawVariant else { return }
        connection.instance?.publish(.diagnostics(Marshal.valueFromVariant(rawVariant)))
    }

    private func publish(_ event: Event) {
        eventSource.yield(event)
    }
