#if canImport(Combine)
import Combine

@MainActor
public final class DeviceListModel: ObservableObject {
    @Published public private(set) var devices: [Device] = []
    @Published public private(set) var discoveryState: DiscoveryState = .discovering

    @frozen
    public enum DiscoveryState: Equatable {
        case discovering
        case ready
    }

    public let manager: DeviceManager

    private var discoveryTask: Task<Void, Never>!

    public init(manager: DeviceManager) {
        self.manager = manager

        discoveryTask = Task { [weak self] in
            guard let self else { return }

            self.devices = await manager.currentDevices()
            self.discoveryState = .ready

            for await snapshot in await manager.snapshots() {
                self.devices = snapshot
            }
        }
    }

    deinit {
        discoveryTask.cancel()
    }
}

#endif
