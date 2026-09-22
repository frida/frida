    public var events: Events {
        eventSource.makeStream()
    }

    public typealias Events = AsyncStream<Event>

    @frozen
    public enum Event {
        case installProgress(phase: PackageInstallPhase, fraction: Double, details: String?)
    }

    private let eventSource = AsyncEventSource<Event>()

    public convenience init() {
        Runtime.ensureInitialized()
        self.init(handle: frida_package_manager_new())
    }

    public var registry: String {
        get {
            String(cString: frida_package_manager_get_registry(handle))
        }
        set {
            frida_package_manager_set_registry(handle, newValue)
        }
    }

    private let onInstallProgress: @convention(c) (OpaquePointer, FridaPackageInstallPhase, gdouble, UnsafePointer<gchar>?, gpointer) -> Void = { _, nativePhase, fraction, rawDetails, userData in
        let connection = Unmanaged<SignalConnection<PackageManager>>.fromOpaque(userData).takeUnretainedValue()
        guard let manager = connection.instance else { return }

        let phase = PackageInstallPhase(rawValue: numericCast(nativePhase.rawValue))!
        let details = rawDetails.map { String(cString: $0) }
        manager.publish(.installProgress(phase: phase, fraction: Double(fraction), details: details))
    }

    private func publish(_ event: Event) {
        eventSource.yield(event)
    }
