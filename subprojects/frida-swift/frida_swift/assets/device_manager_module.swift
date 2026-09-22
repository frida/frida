actor DeviceStore {
    private var devices: [Device] = []
    private let events = AsyncEventSource<[Device]>()
    private let changeEvents = AsyncEventSource<DeviceManager.DeviceChange>()

    private var waiters: [OpaquePointer: [CheckedContinuation<Device, Never>]] = [:]

    func handleInitialSnapshot(_ devices: [Device]) {
        self.devices = devices
        events.yield(self.devices)
    }

    func deviceAppeared(with handle: OpaquePointer) -> Device {
        if let existing = devices.first(where: { $0.handle == handle }) {
            if let continuations = waiters.removeValue(forKey: handle) {
                for c in continuations {
                    c.resume(returning: existing)
                }
            }
            return existing
        }

        g_object_ref(gpointer(handle))
        let device = Device(handle: handle)
        devices.append(device)
        events.yield(devices)
        changeEvents.yield(.appeared(device))

        if let continuations = waiters.removeValue(forKey: handle) {
            for c in continuations {
                c.resume(returning: device)
            }
        }

        return device
    }

    func deviceDisappeared(with handle: OpaquePointer) {
        guard let device = devices.first(where: { $0.handle == handle }) else { return }
        devices.removeAll { $0.handle == handle }
        events.yield(devices)
        changeEvents.yield(.disappeared(device))
    }

    func deviceForHandle(_ handle: OpaquePointer) async -> Device {
        if let existing = devices.first(where: { $0.handle == handle }) {
            return existing
        }

        return await withCheckedContinuation { continuation in
            waiters[handle, default: []].append(continuation)
        }
    }

    func snapshot() -> [Device] {
        devices
    }

    func stream() -> AsyncStream<[Device]> {
        events.makeStream()
    }

    func changeStream() -> AsyncStream<DeviceManager.DeviceChange> {
        changeEvents.makeStream()
    }

    func finish() {
        events.finish()
        changeEvents.finish()
    }
}
